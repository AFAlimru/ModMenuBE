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
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace modmenu::android::overlay {

namespace {

using SwapFn = EGLBoolean (*)(EGLDisplay, EGLSurface);
SwapFn gOriginal = nullptr;

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

EGLBoolean swapDetour(EGLDisplay display, EGLSurface surface) {
    auto& st = state();
    if (st.active.load(std::memory_order_relaxed) && !gFailed && isGameFrame(display, surface)) {
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
    return gOriginal(display, surface);
}

} // namespace

bool install() {
    static bool done = false;
    if (done) return gOriginal != nullptr;
    done      = true;
    void* lib = dlopen("libEGL.so", RTLD_NOW | RTLD_NOLOAD);
    if (!lib) lib = dlopen("libEGL.so", RTLD_NOW);
    if (!lib) return false;
    void* target = dlsym(lib, "eglSwapBuffers");
    if (!target) return false;
    int rc = pl::memory::hook(target, reinterpret_cast<void*>(&swapDetour), reinterpret_cast<void**>(&gOriginal));
    log().info("eglSwapBuffers hook: rc={}", rc);
    return rc == 0 && gOriginal != nullptr;
}

} // namespace modmenu::android::overlay
