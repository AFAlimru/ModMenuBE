#include "EglOverlay.h"

#include "ModMenuMod.h"
#include "State.h"
#include "TouchInput.h"

#include "ui/MenuControl.h"
#include "ui/Ui.h"

#include <pl/Input.hpp>
#include <pl/memory/Hook.hpp>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>
#include <sys/prctl.h>
#include <sys/system_properties.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace modmenu::android::overlay {

namespace {

using SwapFn       = EGLBoolean (*)(EGLDisplay, EGLSurface);
using SwapDamageFn = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLint const*, EGLint);
SwapFn       gOriginal       = nullptr;
SwapDamageFn gOrigDamageKHR  = nullptr; // some drivers/games present with these instead of eglSwapBuffers
SwapDamageFn gOrigDamageEXT  = nullptr;
std::atomic<bool> gVulkan{false};

EGLContext    gContext = EGL_NO_CONTEXT; // EGL context the ImGui GL objects belong to
ImGuiContext* gImGui   = nullptr;
bool          gKeyboardShown = false;
bool          gFailed        = false;
std::chrono::steady_clock::time_point gLastFrame;

// Android density (dp -> px): ro.sf.lcd_density / 160.
float density() {
    static float cached = 0;
    if (cached == 0) {
        char value[PROP_VALUE_MAX]{};
        int  dpi = __system_property_get("ro.sf.lcd_density", value) > 0 ? atoi(value) : 0;
        cached   = dpi > 0 ? dpi / 160.0f : 2.0f;
    }
    return cached;
}

void destroyImGui(bool glAlive) {
    if (!gImGui) return;
    ImGui::SetCurrentContext(gImGui);
    ui::onDeviceLost();
    if (glAlive) {
        ImGui_ImplOpenGL3_Shutdown();
    } else {
        // The GL objects died with their EGL context; deleting their names now could hit the game's objects.
        ImGui::GetIO().BackendRendererUserData = nullptr;
        ImGui::GetIO().BackendRendererName     = nullptr;
    }
    ImGui::DestroyContext(gImGui);
    gImGui   = nullptr;
    gContext = EGL_NO_CONTEXT;
}

bool ensureImGui(EGLContext ctx) {
    if (gImGui && gContext == ctx) return true;
    destroyImGui(false);

    gImGui = ImGui::CreateContext();
    ImGui::SetCurrentContext(gImGui);
    ImGuiIO& io            = ImGui::GetIO();
    io.IniFilename         = nullptr;
    io.LogFilename         = nullptr;
    io.BackendPlatformName = "modmenube_android";
    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    // One layout unit per density-independent pixel: 0.75 * density (the Windows 1080p size) read too small on
    // a phone held at arm's length (tested on a 2.81-density phone, where players picked 1.35x).
    ui::setPlatformScale(std::clamp(density(), 1.0f, 4.0f));
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        log().error("ImGui OpenGL ES 3 backend failed to initialise");
        ImGui::DestroyContext(gImGui);
        gImGui = nullptr;
        return false;
    }
    gContext   = ctx;
    gLastFrame = std::chrono::steady_clock::now();
    auto glStr = [](GLenum e) {
        auto* s = reinterpret_cast<char const*>(glGetString(e));
        return s ? s : "?";
    };
    log().info("ImGui ready: density {:.2f}, GL {} / {}", density(), glStr(GL_VERSION), glStr(GL_RENDERER));
    return true;
}

// Touch screens have no wheel: a vertical drag scrolls the window under the finger. Once the drag passes the
// threshold the pressed widget is released, so lifting the finger over a button does not click it.
void touchScroll() {
    ImGuiContext&       g  = *GImGui;
    ImGuiIO&            io = g.IO;
    static ImGuiWindow* target    = nullptr;
    static bool         scrolling = false;
    static ImVec2       start;
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        target    = nullptr;
        scrolling = false;
        start     = io.MousePos;
        for (ImGuiWindow* w = g.HoveredWindow; w; w = w->ParentWindow) {
            if (w->ScrollMax.y > 0) {
                target = w;
                break;
            }
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        target    = nullptr;
        scrolling = false;
        return;
    }
    if (!target) return;
    float dx = io.MousePos.x - start.x, dy = io.MousePos.y - start.y;
    bool  editingText = g.ActiveId != 0 && g.InputTextState.ID == g.ActiveId;
    if (!scrolling && !editingText && std::fabs(dy) > 10.0f * density() && std::fabs(dy) > std::fabs(dx)) {
        scrolling = true;
        ImGui::ClearActiveID();
    }
    if (scrolling && io.MouseDelta.y != 0)
        ImGui::SetScrollY(target, std::clamp(target->Scroll.y - io.MouseDelta.y, 0.0f, target->ScrollMax.y));
}

void renderFrame(EGLDisplay display, EGLSurface surface) {
    EGLContext ctx = eglGetCurrentContext();
    if (ctx == EGL_NO_CONTEXT) return;
    EGLint w = 0, h = 0;
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);
    if (w <= 0 || h <= 0) return;

    if (!ensureImGui(ctx)) {
        gFailed = true;
        return;
    }
    ImGui::SetCurrentContext(gImGui);
    ImGuiIO& io = ImGui::GetIO();

    auto now     = std::chrono::steady_clock::now();
    io.DeltaTime = std::max(std::chrono::duration<float>(now - gLastFrame).count(), 1e-4f);
    gLastFrame   = now;
    io.DisplaySize             = ImVec2(float(w), float(h));
    io.DisplayFramebufferScale = ImVec2(1, 1);

    input::drain();
    if (ui::prepareFrame(float(w), float(h))) { // font atlas rebuilt
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        GLint maxTex = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);
        ImFontAtlas* atlas = io.Fonts;
        if (!atlas->IsBuilt() || atlas->TexWidth <= 0 || atlas->TexHeight <= 0 || atlas->TexWidth > maxTex
            || atlas->TexHeight > maxTex) {
            log().error("font atlas unusable ({}x{}, GL max {}); menu disabled", atlas->TexWidth, atlas->TexHeight, maxTex);
            gFailed = true;
            menuClosedByUi();
            return;
        }
        ImGui_ImplOpenGL3_CreateFontsTexture();
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    touchScroll();
    ui::drawFrame();
    ImGui::Render();

    GLint drawFbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
    if (drawFbo != 0) glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (drawFbo != 0) glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLuint(drawFbo));

    // After a finger lifts, move the virtual mouse away so nothing stays "hovered".
    if (input::consumeReleasedHover()) io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);

    bool wantKeyboard = io.WantTextInput;
    if (wantKeyboard != gKeyboardShown) {
        gKeyboardShown = wantKeyboard;
        if (wantKeyboard) pl::input::showKeyboard();
        else pl::input::hideKeyboard();
    }
}

// eglSwapBuffers is also called by Android's own UI renderer (HWUI "RenderThread") for the launcher's floating
// buttons and panels, and possibly by other surfaces. Only the game's frame may carry the menu: skip HWUI, and
// stick to the thread that presents the largest surface.
bool isGameFrame(EGLDisplay display, EGLSurface surface) {
    thread_local int isHwui = -1;
    if (isHwui < 0) {
        char name[17]{};
        prctl(PR_GET_NAME, name, 0, 0, 0);
        isHwui = std::string_view(name) == "RenderThread" ? 1 : 0;
    }
    if (isHwui) return false;
    EGLint w = 0, h = 0;
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);
    static long long bestArea = 0;
    long long        area     = (long long)w * h;
    if (area < bestArea * 9 / 10) return false; // a smaller, secondary surface
    if (area > bestArea) bestArea = area;
    return true;
}

// Logs the first present of each thread (name, size, whether it counts as the game frame): what a tester's log
// needs to show when the menu does not appear.
void logPresenter(char const* api, EGLDisplay display, EGLSurface surface, bool accepted) {
    thread_local bool logged = false;
    static std::atomic<int> threads{0};
    if (logged || threads.load() >= 12) return;
    logged = true;
    threads.fetch_add(1);
    char name[17]{};
    prctl(PR_GET_NAME, name, 0, 0, 0);
    EGLint w = 0, h = 0;
    if (display != EGL_NO_DISPLAY) {
        eglQuerySurface(display, surface, EGL_WIDTH, &w);
        eglQuerySurface(display, surface, EGL_HEIGHT, &h);
    }
    log().info("{} on thread '{}' ({}x{}){}", api, name, w, h, accepted ? ", used for the menu" : "");
}

// libEGL may implement one swap entry point with another: draw once per frame.
thread_local bool tInsideSwap = false;
struct SwapGuard {
    bool outer;
    SwapGuard() : outer(!tInsideSwap) { tInsideSwap = true; }
    ~SwapGuard() {
        if (outer) tInsideSwap = false;
    }
};

void beforeSwap(char const* api, EGLDisplay display, EGLSurface surface) {
    auto& st = state();
    bool  game = st.active.load(std::memory_order_relaxed) && !gFailed && isGameFrame(display, surface);
    logPresenter(api, display, surface, game);
    if (game) {
        if (st.menuOpen.load(std::memory_order_relaxed) || ui::wantsFrame()) {
            renderFrame(display, surface);
            st.lastFrameMs.store(nowMs());
            // The menu closed itself (close button, Back/Esc on the list).
            if (st.menuOpen.load() && !ui::wantsFrame()) menuClosedByUi();
        }
        if (!st.menuOpen.load(std::memory_order_relaxed) && gKeyboardShown) {
            gKeyboardShown = false;
            pl::input::hideKeyboard();
        }
    }
}

EGLBoolean swapDetour(EGLDisplay display, EGLSurface surface) {
    SwapGuard guard;
    if (guard.outer) beforeSwap("eglSwapBuffers", display, surface);
    return gOriginal(display, surface);
}

EGLBoolean swapDamageKHRDetour(EGLDisplay display, EGLSurface surface, EGLint const* rects, EGLint n) {
    SwapGuard guard;
    if (guard.outer) beforeSwap("eglSwapBuffersWithDamageKHR", display, surface);
    return gOrigDamageKHR(display, surface, rects, n);
}

EGLBoolean swapDamageEXTDetour(EGLDisplay display, EGLSurface surface, EGLint const* rects, EGLint n) {
    SwapGuard guard;
    if (guard.outer) beforeSwap("eglSwapBuffersWithDamageEXT", display, surface);
    return gOrigDamageEXT(display, surface, rects, n);
}

// Vulkan is only watched, not drawn into: a game that presents with it gets a clear message instead of a menu
// that silently never appears.
using VkPresentFn   = int (*)(void* queue, void const* info);
using VkDeviceProcFn = void* (*)(void* device, char const* name);
VkPresentFn    gOrigVkPresent   = nullptr;
VkDeviceProcFn gOrigVkDeviceProc = nullptr;

void noteVulkan(char const* how) {
    if (gVulkan.load(std::memory_order_relaxed)) return;
    char name[17]{};
    prctl(PR_GET_NAME, name, 0, 0, 0);
    if (std::string_view(name) == "RenderThread") return; // Android's own UI renderer may use Vulkan
    if (gVulkan.exchange(true)) return;
    log().warn("the game renders with Vulkan ({}, thread '{}'); the menu can only draw over OpenGL ES", how, name);
}

int vkPresentDetour(void* queue, void const* info) {
    noteVulkan("vkQueuePresentKHR");
    return gOrigVkPresent(queue, info);
}

void* vkDeviceProcDetour(void* device, char const* name) {
    if (name && std::string_view(name) == "vkQueuePresentKHR") noteVulkan("vkGetDeviceProcAddr");
    return gOrigVkDeviceProc(device, name);
}

template <class Fn>
bool hookSymbol(void* lib, char const* symbol, void* detour, Fn* original) {
    void* target = lib ? dlsym(lib, symbol) : nullptr;
    if (!target) return false;
    int rc = pl::memory::hook(target, detour, reinterpret_cast<void**>(original));
    log().info("{} hook: rc={}", symbol, rc);
    return rc == 0 && *original;
}

} // namespace

bool gameUsesVulkan() { return gVulkan.load(); }

bool install() {
    static bool done = false;
    if (done) return gOriginal != nullptr;
    done      = true;
    void* lib = dlopen("libEGL.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) lib = dlopen("libEGL.so", RTLD_NOW);
    if (!lib) return false;
    bool ok = hookSymbol(lib, "eglSwapBuffers", reinterpret_cast<void*>(&swapDetour), &gOriginal);
    hookSymbol(lib, "eglSwapBuffersWithDamageKHR", reinterpret_cast<void*>(&swapDamageKHRDetour), &gOrigDamageKHR);
    hookSymbol(lib, "eglSwapBuffersWithDamageEXT", reinterpret_cast<void*>(&swapDamageEXTDetour), &gOrigDamageEXT);
    if (void* vk = dlopen("libvulkan.so", RTLD_NOW)) {
        hookSymbol(vk, "vkQueuePresentKHR", reinterpret_cast<void*>(&vkPresentDetour), &gOrigVkPresent);
        hookSymbol(vk, "vkGetDeviceProcAddr", reinterpret_cast<void*>(&vkDeviceProcDetour), &gOrigVkDeviceProc);
    }
    return ok;
}

} // namespace modmenu::android::overlay
