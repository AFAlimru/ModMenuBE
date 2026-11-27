#include "overlay/Overlay.h"
#include "overlay/D3D12Runtime.h"
#include "overlay/SwapChainQueue.h"
#include "overlay/GpuCompletion.h"
#include "overlay/OpenDiagnostics.h"

#include "core/Log.h"
#include "core/Util.h"
#include "ui/Fonts.h"
#include "ui/MenuControl.h"
#include "ui/Ui.h"
#include "ui/LegacySettings.h"

#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <MinHook.h>
#include <imm.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <atomic>
#include <mutex>
#include <map>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace modmenu::overlay {

namespace {

template <class T>
void safeRelease(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}

using PresentFn        = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1Fn       = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, DXGI_PRESENT_PARAMETERS const*);
using ResizeBuffersFn  = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);
using ResizeBuffers1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT, DXGI_FORMAT, UINT, UINT const*,
                                                     IUnknown* const*);
using CreateSwapChainFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*, IUnknown*, DXGI_SWAP_CHAIN_DESC*,
                                                      IDXGISwapChain**);
using CreateHwndFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, HWND,
                                                 DXGI_SWAP_CHAIN_DESC1 const*,
                                                 DXGI_SWAP_CHAIN_FULLSCREEN_DESC const*, IDXGIOutput*,
                                                 IDXGISwapChain1**);
using CreateCoreFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*, IUnknown*,
                                                 DXGI_SWAP_CHAIN_DESC1 const*, IDXGIOutput*,
                                                 IDXGISwapChain1**);
using CreateCompositionFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*, IUnknown*,
                                                        DXGI_SWAP_CHAIN_DESC1 const*, IDXGIOutput*,
                                                        IDXGISwapChain1**);
using SetCursorPosFn        = BOOL(WINAPI*)(int, int);
using ClipCursorFn          = BOOL(WINAPI*)(RECT const*);

constexpr size_t kPresentIndex             = 8;
constexpr size_t kResizeBuffersIndex       = 13;
constexpr size_t kPresent1Index            = 22;
constexpr size_t kResizeBuffers1Index      = 39;

PresentFn             oPresent{};
Present1Fn            oPresent1{};
ResizeBuffersFn       oResizeBuffers{};
ResizeBuffers1Fn      oResizeBuffers1{};
CreateSwapChainFn oCreateSwapChain{};
CreateHwndFn oCreateHwnd{};
CreateCoreFn oCreateCore{};
CreateCompositionFn oCreateComposition{};
SetCursorPosFn        oSetCursorPos{};
ClipCursorFn          oClipCursor{};

std::vector<void*> gHookTargets;
std::vector<void*> gCreationHookTargets;
std::recursive_mutex gInstallMutex; // worker lifecycle, never taken by callbacks
bool gObservingCreation{};          // guarded by gInstallMutex
std::atomic<bool>  gInstalled{false};
std::atomic<bool> gInvalidateBuffers{false}; // window thread publishes only
std::recursive_mutex gRenderMutex; // frame rendering vs. teardown (resize / F11 / uninstall)

// Graphics state, guarded by gRenderMutex.
IDXGISwapChain*      gSwapChain{};
ID3D11Device*        gDevice{};
ID3D11DeviceContext* gContext{};
ID3D11On12Device*    gDevice11On12{};
bool                 gIsD3D12 = false;
bool                 gImGuiReady = false;
std::atomic<bool> gGraphicsFailed{false};
// The DLL is pinned. An unresolved GPU marker must also survive CRT detach;
// explicit successful teardown resets its resources, process exit reclaims it.
GpuCompletion& gCompletion = *new GpuCompletion;

struct BackBuffer {
    ID3D11Resource*         resource{}; // wrapped (D3D12) or plain (D3D11)
    ID3D11RenderTargetView* rtv{};
};
std::vector<BackBuffer> gBuffers;

std::atomic<HWND> gWindow{};
// A later plugin may chain our old procedure. Keep its per-window predecessor
// until that window is destroyed instead of redirecting it to another window.
std::mutex gWindowMutex;
std::map<HWND, WNDPROC> gWindowProcedures;

// ImGui is not thread-safe: the window thread queues messages, the render thread feeds them to ImGui.
struct QueuedMessage {
    UINT   msg;
    WPARAM wParam;
    LPARAM lParam;
};
std::mutex                 gMessageMutex;
std::vector<QueuedMessage> gMessages;

// Bedrock keeps the IME detached from its window outside its own text fields. While one of our text
// fields has focus we attach the default IME context (window thread), and detach it again afterwards.
constexpr UINT kImeMessage = WM_APP + 0x4D4D;
bool           gImeRequested = false; // render thread: last state posted
bool           gImeForced    = false; // window thread: we attached the context

void setImeEnabled(HWND hwnd, bool on) {
    if (on) {
        if (HIMC himc = ImmGetContext(hwnd)) { // the game already has one attached
            ImmReleaseContext(hwnd, himc);
            return;
        }
        gImeForced = ImmAssociateContextEx(hwnd, nullptr, IACE_DEFAULT) != FALSE;
        log::info("IME context attached for text input: {}", gImeForced);
    } else if (gImeForced) {
        ImmAssociateContext(hwnd, nullptr);
        gImeForced = false;
    }
}

void requestIme(bool on) {
    if (on == gImeRequested || !gWindow) return;
    gImeRequested = on;
    PostMessageW(gWindow, kImeMessage, on ? 1 : 0, 0);
}

void queueMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    std::lock_guard lock(gMessageMutex);
    if (gMessages.size() < 4096) gMessages.push_back({msg, wParam, lParam});
}

void drainMessages() {
    std::vector<QueuedMessage> pending;
    {
        std::lock_guard lock(gMessageMutex);
        pending.swap(gMessages);
    }
    for (auto const& m : pending) {
        if (!ui::acceptsUiInput())
            continue;
        if (m.msg == WM_KEYDOWN || m.msg == WM_SYSKEYDOWN || m.msg == WM_KEYUP || m.msg == WM_SYSKEYUP) {
            unsigned key = unsigned(m.wParam);
            bool extended = (m.lParam & (1 << 24)) != 0;
            if (key == VK_SHIFT)
                key = MapVirtualKeyW((unsigned(m.lParam) >> 16) & 255, MAPVK_VSC_TO_VK_EX);
            else if (key == VK_CONTROL)
                key = extended ? VK_RCONTROL : VK_LCONTROL;
            else if (key == VK_MENU)
                key = extended ? VK_RMENU : VK_LMENU;
            if (ui::legacy::captureKey(key, m.msg == WM_KEYDOWN || m.msg == WM_SYSKEYDOWN))
                continue;
        }
        if (ui::legacy::capturing() && (m.msg == WM_CHAR || m.msg == WM_SYSCHAR || m.msg == WM_UNICHAR))
            continue;
        if (m.msg == WM_CHAR && m.wParam >= 0x80 && (m.wParam < 0xD800 || m.wParam > 0xDFFF)) {
            wchar_t w = wchar_t(m.wParam);
            char    utf8[8]{};
            if (int n = WideCharToMultiByte(CP_UTF8, 0, &w, 1, utf8, sizeof(utf8), nullptr, nullptr); n > 0)
                ui::fonts::note(std::string_view(utf8, size_t(n))); // typed CJK outside the preloaded set
        }
        ImGui_ImplWin32_WndProcHandler(gWindow, m.msg, m.wParam, m.lParam);
    }
}

// Mouse buttons straight from the input state. Another mod's window procedure installed after
// ours may keep button messages from reaching it (the menu then hovers but never clicks), so
// while the menu is drawn and the game window is in front the buttons are read every frame.
// ImGui drops repeated states, so buttons that also arrive as messages are not doubled.
// Skipped while UI input is suspended (focus return waiting for release, transient capture).
void pollMouseButtons() {
    if (!ui::acceptsUiInput())
        return;
    HWND const fg = GetForegroundWindow();
    if (!gWindow || (fg != gWindow && !IsChild(fg, gWindow) && !IsChild(gWindow, fg)))
        return;
    bool const swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0; // the async state is the physical button
    int const left = swapped ? VK_RBUTTON : VK_LBUTTON;
    int const right = swapped ? VK_LBUTTON : VK_RBUTTON;
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseButtonEvent(0, (GetAsyncKeyState(left) & 0x8000) != 0);
    io.AddMouseButtonEvent(1, (GetAsyncKeyState(right) & 0x8000) != 0);
    io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
}

LRESULT CALLBACK wndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Logs which module's window procedure sits in front of ours (it sees every message first).
void reportWindowChain() {
    static LONG_PTR last = 0;
    LONG_PTR const top = GetWindowLongPtrW(gWindow, GWLP_WNDPROC);
    if (top == last)
        return;
    last = top;
    if (top == reinterpret_cast<LONG_PTR>(&wndProcHook)) {
        log::info("window procedure: ModMenuBE is first in the chain");
        return;
    }
    HMODULE owner = nullptr;
    wchar_t name[MAX_PATH]{};
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(top), &owner))
        GetModuleFileNameW(owner, name, MAX_PATH);
    log::info("window procedure: {} is in front of ModMenuBE (sees messages first)", toUtf8(name));
}

HRESULT releaseBackBuffers() {
    // F11 with no retained buffers must never enter the graphics driver.
    if (gBuffers.empty())
        return S_OK;
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::BufferCleanup);
    if (!gContext)
        return E_UNEXPECTED;
    gContext->OMSetRenderTargets(0, nullptr, nullptr);
    gContext->ClearState();
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::GpuCompletion);
    const auto completed = gCompletion.wait();
    if (FAILED(completed)) {
        if (!gGraphicsFailed.exchange(true))
            log::error(
                "Overlay cleanup refused: GPU completion 0x{:08X}; retained buffers, drawing/input disabled",
                unsigned(completed));
        return completed;
    }
    for (auto& b : gBuffers) {
        safeRelease(b.rtv);
        safeRelease(b.resource);
    }
    gBuffers.clear();
    // Flush deferred D3D11/On12 destruction only after GPU completion and
    // dropping our references, before forwarding an actual DXGI resize.
    gContext->Flush();
    return S_OK;
}

bool releaseGraphics() {
    std::lock_guard lock(gRenderMutex);
    if (FAILED(releaseBackBuffers()))
        return false;
    if (gImGuiReady) {
        ui::onDeviceLost();
        ImGui_ImplDX11_Shutdown();
        gImGuiReady = false;
    }
    safeRelease(gDevice11On12);
    safeRelease(gContext);
    safeRelease(gDevice);
    gCompletion.reset();
    gSwapChain = nullptr;
    return true;
}

bool createBackBuffers(IDXGISwapChain* swapChain) {
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc))) return false;
    if (!gIsD3D12) {
        BackBuffer b;
        ID3D11Texture2D* tex{};
        if (FAILED(swapChain->GetBuffer(0, IID_PPV_ARGS(&tex)))) return false;
        HRESULT hr = gDevice->CreateRenderTargetView(tex, nullptr, &b.rtv);
        b.resource = tex;
        gBuffers.push_back(b);
        return SUCCEEDED(hr);
    }
    for (UINT i = 0; i < desc.BufferCount; ++i) {
        ID3D12Resource* buffer12{};
        if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&buffer12)))) return false;
        BackBuffer       b;
        D3D11_RESOURCE_FLAGS flags{D3D11_BIND_RENDER_TARGET, 0, 0, 0};
        HRESULT hr = gDevice11On12->CreateWrappedResource(buffer12, &flags, D3D12_RESOURCE_STATE_PRESENT,
                                                          D3D12_RESOURCE_STATE_PRESENT, IID_PPV_ARGS(&b.resource));
        buffer12->Release();
        if (FAILED(hr)) return false;
        if (FAILED(gDevice->CreateRenderTargetView(b.resource, nullptr, &b.rtv))) {
            b.resource->Release();
            return false;
        }
        gBuffers.push_back(b);
    }
    return !gBuffers.empty();
}

bool initGraphics(IDXGISwapChain* swapChain) {
    gSwapChain = swapChain;
    ID3D12Device* device12{};
    if (SUCCEEDED(swapChain->GetDevice(IID_PPV_ARGS(&device12)))) {
        gIsD3D12 = true;
        // GetDevice returns the D3D12 device, not the creation command queue.
        // Only the successful DXGI creation/ResizeBuffers1 arguments establish ownership.
        auto queue = swapchain::queue(swapChain);
        if (!queue) {
            device12->Release();
            static std::atomic<bool> warned{};
            if (!warned.exchange(true))
                log::warn("D3D12 overlay waiting: swapchain creation queue was not observed; refusing "
                          "first-queue fallback");
            return false;
        }
        IUnknown* queues[] = {queue.Get()};
        HRESULT   hr = D3D11On12CreateDevice(device12, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, queues, 1, 0,
                                             &gDevice, &gContext, nullptr);
        device12->Release();
        if (FAILED(hr) || FAILED(gDevice->QueryInterface(IID_PPV_ARGS(&gDevice11On12)))) {
            log::error("D3D11On12CreateDevice failed: 0x{:08X}", unsigned(hr));
            gGraphicsFailed = true;
            return false;
        }
        hr = gCompletion.initialize(gDevice, gContext, queue.Get());
        if (FAILED(hr)) {
            log::error("D3D12 overlay completion initialization refused: 0x{:08X}", unsigned(hr));
            gGraphicsFailed = true;
            return false;
        }
    } else {
        gIsD3D12 = false;
        if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&gDevice)))) {
            log::error("swap chain has neither a D3D12 nor a D3D11 device");
            gGraphicsFailed = true;
            return false;
        }
        gDevice->GetImmediateContext(&gContext);
        auto hr = gCompletion.initialize(gDevice, gContext);
        if (FAILED(hr)) {
            log::error("D3D11 overlay completion initialization refused: 0x{:08X}", unsigned(hr));
            gGraphicsFailed = true;
            return false;
        }
    }
    if (!ImGui_ImplDX11_Init(gDevice, gContext)) {
        log::error("ImGui_ImplDX11_Init failed");
        gGraphicsFailed = true;
        return false;
    }
    gImGuiReady = true;
    log::info("renderer ready ({})", gIsD3D12 ? "D3D12 via D3D11On12" : "D3D11");
    return true;
}

LRESULT CALLBACK wndProcHook(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WNDPROC previous{};
    {
        std::lock_guard lock(gWindowMutex);
        auto found = gWindowProcedures.find(hwnd);
        if (found != gWindowProcedures.end()) {
            previous = found->second;
            if (msg == WM_NCDESTROY)
                gWindowProcedures.erase(found);
        }
    }
    auto forward = [&] {
        return previous ? CallWindowProcW(previous, hwnd, msg, wParam, lParam)
                        : DefWindowProcW(hwnd, msg, wParam, lParam);
    };
    if (msg == WM_NCDESTROY) {
        auto selected = hwnd;
        if (gWindow.compare_exchange_strong(selected, nullptr))
            ui::notifyFocusLost();
    }
    if (!gInstalled || hwnd != gWindow || msg == WM_NCDESTROY)
        return forward();
    diagnostics::Scope diagnostic(diagnostics::Channel::Window, diagnostics::Stage::WindowDispatch);
    if (msg == WM_KILLFOCUS || (msg == WM_ACTIVATEAPP && !wParam)) {
        ui::notifyFocusLost();
        std::lock_guard lock(gMessageMutex);
        gMessages.clear();
    }
    if (msg == kImeMessage) {
        setImeEnabled(hwnd, wParam != 0);
        return 0;
    }
    diagnostic.stage(diagnostics::Stage::WindowInput);
    if (!gGraphicsFailed && ui::handleWindowMessage(msg, wParam, lParam))
        return 0;

    if (!gGraphicsFailed && ui::acceptsUiInput() && msg != WM_SETCURSOR && msg != WM_INPUT)
        queueMessage(msg, wParam, lParam);
    if (!gGraphicsFailed && ui::blocksGameInput()) {
        switch (msg) {
        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(LoadCursorW(nullptr, IDC_ARROW));
                return TRUE;
            }
            break;
        case WM_INPUT:
            // Raw mouse/keyboard: keep it from the game but let Windows clean up.
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        case WM_XBUTTONDBLCLK:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_UNICHAR:
        case WM_IME_CHAR:
        case WM_IME_COMPOSITION:
        case WM_IME_STARTCOMPOSITION:
        case WM_IME_ENDCOMPOSITION:
            if (msg == WM_SYSKEYDOWN && wParam == VK_F4) break; // keep Alt+F4
            // Composition and WM_IME_CHAR go to DefWindowProc: it turns the committed text into the WM_CHAR
            // messages ImGui reads (returning 0 for WM_IME_CHAR used to drop every IME character).
            return msg == WM_IME_COMPOSITION || msg == WM_IME_STARTCOMPOSITION || msg == WM_IME_ENDCOMPOSITION
                        || msg == WM_IME_CHAR
                     ? DefWindowProcW(hwnd, msg, wParam, lParam)
                     : 0;
        default: break;
        }
    }
    // No context calls, resource release, GPU waits or render mutex acquisition
    // from the F11 window callback. A render/resize boundary consumes the flag.
    if (msg == WM_KEYDOWN && wParam == VK_F11)
        gInvalidateBuffers = true;
    diagnostic.stage(diagnostics::Stage::WindowOriginal);
    return forward();
}

bool attachWindow(HWND hwnd) {
    DWORD process{};
    if (!hwnd || !IsWindow(hwnd) || !GetWindowThreadProcessId(hwnd, &process) ||
        process != GetCurrentProcessId())
        return false;
    if (gWindow == hwnd)
        return true;
    {
        std::lock_guard lock(gWindowMutex);
        if (!gWindowProcedures.contains(hwnd)) {
            if (gWindowProcedures.size() >= 8)
                return false;
            SetLastError(0);
            auto previous = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndProcHook));
            auto error = GetLastError();
            if (!previous) {
                log::error("window input attachment failed; win32={}", error);
                return false;
            }
            gWindowProcedures.emplace(hwnd, reinterpret_cast<WNDPROC>(previous));
        }
        if (auto old = gWindow.load()) {
            auto found = gWindowProcedures.find(old);
            if (found != gWindowProcedures.end() &&
                GetWindowLongPtrW(old, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(&wndProcHook)) {
                if (SetWindowLongPtrW(old, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(found->second)))
                    gWindowProcedures.erase(found);
            }
        }
    }
    if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendPlatformUserData) {
        ui::notifyFocusLost();
        {
            std::lock_guard lock(gMessageMutex);
            gMessages.clear();
        }
        ImGui_ImplWin32_Shutdown();
    }
    if (!ImGui::GetCurrentContext()) {
        ImGui::CreateContext();
        ImGuiIO& io    = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }
    gWindow = hwnd;
    if (!ImGui_ImplWin32_Init(hwnd)) {
        gGraphicsFailed = true;
        log::error("window input backend initialization failed");
        return false;
    }
    char className[128]{};
    GetClassNameA(hwnd, className, sizeof className);
    log::info("attached to input window {:p}; class={}; foreground={}", static_cast<void*>(hwnd), className,
              GetForegroundWindow() == hwnd);
    return true;
}

void renderFrame(IDXGISwapChain* swapChain) {
    std::lock_guard lock(gRenderMutex);
    DXGI_SWAP_CHAIN_DESC desc{};
    if (FAILED(swapChain->GetDesc(&desc)) || !IsWindow(desc.OutputWindow))
        return;
    auto selected = gWindow.load();
    if (desc.OutputWindow != selected) {
        auto foreground = GetForegroundWindow();
        // Do not let subsequent auxiliary/plugin Presents steal the selected
        // window. A foreground game output can replace the early probe.
        bool foregroundOutput =
            foreground && GetAncestor(desc.OutputWindow, GA_ROOT) == GetAncestor(foreground, GA_ROOT);
        if (IsWindow(selected) && !foregroundOutput)
            return;
        if (gSwapChain && !releaseGraphics())
            return;
        if (!attachWindow(desc.OutputWindow))
            return;
    }
    if (gGraphicsFailed)
        return;
    ui::pollInputRelease();
    if (gInvalidateBuffers.exchange(false) && FAILED(releaseBackBuffers()))
        return;
    if (!ui::wantsFrame()) {
        requestIme(false);
        {
            std::lock_guard lock(gMessageMutex);
            gMessages.clear();
        }
        // Hold no back buffer references while the menu is closed.
        releaseBackBuffers();
        return;
    }
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::RenderLock);
    if (gSwapChain && gSwapChain != swapChain && !releaseGraphics())
        return;
    if (!gWindow) return;
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::GraphicsInit);
    if (!gImGuiReady && !initGraphics(swapChain)) return;
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::BackBuffers);
    if (gBuffers.empty() && !createBackBuffers(swapChain)) {
        releaseBackBuffers();
        return;
    }

    size_t index = 0;
    if (gIsD3D12) {
        IDXGISwapChain3* sc3{};
        if (FAILED(swapChain->QueryInterface(IID_PPV_ARGS(&sc3)))) return;
        index = sc3->GetCurrentBackBufferIndex();
        sc3->Release();
        if (index >= gBuffers.size()) {
            releaseBackBuffers();
            return;
        }
    }

    RECT rc{};
    GetClientRect(gWindow, &rc);
    float width  = float(rc.right - rc.left);
    float height = float(rc.bottom - rc.top);
    if (width <= 0 || height <= 0) return;

    diagnostics::clear(diagnostics::Channel::Request);
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::PrepareUi);
    if (ui::prepareFrame(width, height)) ImGui_ImplDX11_InvalidateDeviceObjects();
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::FontTexture);
    ImGui_ImplDX11_NewFrame();
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::Messages);
    drainMessages();
    pollMouseButtons();
    reportWindowChain();
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::Win32Frame);
    ImGui_ImplWin32_NewFrame();
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::ImGuiFrame);
    ImGui::NewFrame();
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::DrawUi);
    ui::drawFrame();
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::BuildDrawData);
    ImGui::Render();
    requestIme(ImGui::GetIO().WantTextInput);

    auto& buffer = gBuffers[index];
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::AcquireWrapped);
    if (gIsD3D12) gDevice11On12->AcquireWrappedResources(&buffer.resource, 1);
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::BindTarget);
    gContext->OMSetRenderTargets(1, &buffer.rtv, nullptr);
    D3D11_VIEWPORT vp{0, 0, width, height, 0, 1};
    gContext->RSSetViewports(1, &vp);
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::DrawGpu);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::ReleaseWrapped);
    if (gIsD3D12) gDevice11On12->ReleaseWrappedResources(&buffer.resource, 1);
    diagnostics::enter(diagnostics::Channel::Render, diagnostics::Stage::Flush);
    gContext->Flush();
}

HRESULT STDMETHODCALLTYPE presentHook(IDXGISwapChain* swapChain, UINT sync, UINT flags) {
    diagnostics::Scope diagnostic(diagnostics::Channel::Render, diagnostics::Stage::RenderLock);
    if (gInstalled && !(flags & DXGI_PRESENT_TEST))
        renderFrame(swapChain);
    // DXGI Present may synchronously message the window thread. Never retain
    // our resource mutex across that call; Resize already serializes our draw.
    diagnostic.stage(diagnostics::Stage::Present);
    return oPresent(swapChain, sync, flags);
}

HRESULT STDMETHODCALLTYPE present1Hook(IDXGISwapChain1* swapChain, UINT sync, UINT flags,
                                       DXGI_PRESENT_PARAMETERS const* params) {
    diagnostics::Scope diagnostic(diagnostics::Channel::Render, diagnostics::Stage::RenderLock);
    if (gInstalled && !(flags & DXGI_PRESENT_TEST))
        renderFrame(swapChain);
    diagnostic.stage(diagnostics::Stage::Present);
    return oPresent1(swapChain, sync, flags, params);
}

HRESULT STDMETHODCALLTYPE resizeBuffersHook(IDXGISwapChain* swapChain, UINT count, UINT w, UINT h, DXGI_FORMAT fmt,
                                            UINT flags) {
    std::lock_guard lock(gRenderMutex);
    if (swapChain == gSwapChain) {
        auto hr = releaseBackBuffers();
        if (FAILED(hr))
            return hr;
        gInvalidateBuffers = false;
    }
    return oResizeBuffers(swapChain, count, w, h, fmt, flags);
}

HRESULT STDMETHODCALLTYPE resizeBuffers1Hook(IDXGISwapChain3* swapChain, UINT count, UINT w, UINT h, DXGI_FORMAT fmt,
                                             UINT flags, UINT const* mask, IUnknown* const* queues) {
    std::lock_guard lock(gRenderMutex);
    if (swapChain == gSwapChain) {
        auto hr = releaseBackBuffers();
        if (FAILED(hr))
            return hr;
        gInvalidateBuffers = false;
    }
    auto hr = oResizeBuffers1(swapChain, count, w, h, fmt, flags, mask, queues);
    if (SUCCEEDED(hr) && queues) {
        DXGI_SWAP_CHAIN_DESC desc{};
        auto n = count;
        if (!n && SUCCEEDED(swapChain->GetDesc(&desc)))
            n = desc.BufferCount;
        Microsoft::WRL::ComPtr<IUnknown> owner;
        bool one =
            n > 0 && n <= 16 && queues[0] && SUCCEEDED(queues[0]->QueryInterface(IID_PPV_ARGS(&owner)));
        for (UINT i = 1; one && i < n; ++i) {
            Microsoft::WRL::ComPtr<IUnknown> current;
            one = queues[i] && SUCCEEDED(queues[i]->QueryInterface(IID_PPV_ARGS(&current))) &&
                  current.Get() == owner.Get();
        }
        // Recreate the On12 device after a queue change; do not retain its old queue.
        if (gSwapChain == swapChain)
            releaseGraphics();
        swapchain::clear(swapChain);
        if (one)
            swapchain::bind(swapChain, owner.Get());
    }
    return hr;
}

void recordCreated(HRESULT hr, IUnknown* device, IDXGISwapChain* chain) {
    if (SUCCEEDED(hr) && chain && swapchain::bind(chain, device))
        log::info("D3D12 swapchain creation queue recorded");
}
HRESULT STDMETHODCALLTYPE createSwapChainHook(IDXGIFactory* factory, IUnknown* device,
                                              DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out) {
    auto hr = oCreateSwapChain(factory, device, desc, out);
    recordCreated(hr, device, SUCCEEDED(hr) && out ? *out : nullptr);
    return hr;
}
HRESULT STDMETHODCALLTYPE createHwndHook(IDXGIFactory2* factory, IUnknown* device, HWND window,
                                         DXGI_SWAP_CHAIN_DESC1 const* desc,
                                         DXGI_SWAP_CHAIN_FULLSCREEN_DESC const* full, IDXGIOutput* restrict,
                                         IDXGISwapChain1** out) {
    auto hr = oCreateHwnd(factory, device, window, desc, full, restrict, out);
    recordCreated(hr, device, SUCCEEDED(hr) && out ? *out : nullptr);
    return hr;
}
HRESULT STDMETHODCALLTYPE createCoreHook(IDXGIFactory2* factory, IUnknown* device, IUnknown* window,
                                         DXGI_SWAP_CHAIN_DESC1 const* desc, IDXGIOutput* restrict,
                                         IDXGISwapChain1** out) {
    auto hr = oCreateCore(factory, device, window, desc, restrict, out);
    recordCreated(hr, device, SUCCEEDED(hr) && out ? *out : nullptr);
    return hr;
}
HRESULT STDMETHODCALLTYPE createCompositionHook(IDXGIFactory2* factory, IUnknown* device,
                                                DXGI_SWAP_CHAIN_DESC1 const* desc, IDXGIOutput* restrict,
                                                IDXGISwapChain1** out) {
    auto hr = oCreateComposition(factory, device, desc, restrict, out);
    recordCreated(hr, device, SUCCEEDED(hr) && out ? *out : nullptr);
    return hr;
}

// While the menu holds the mouse, the game must not re-center or confine it.
BOOL WINAPI setCursorPosHook(int x, int y) {
    if (!gGraphicsFailed && ui::blocksGameInput())
        return TRUE;
    return oSetCursorPos(x, y);
}

BOOL WINAPI clipCursorHook(RECT const* rect) {
    if (!gGraphicsFailed && ui::blocksGameInput())
        return oClipCursor(nullptr);
    return oClipCursor(rect);
}

bool hook(void* target, void* detour, void** original, std::vector<void*>& targets = gHookTargets) {
    if (!target) return false;
    if (MH_CreateHook(target, detour, original) != MH_OK) return false;
    if (MH_EnableHook(target) != MH_OK) {
        MH_RemoveHook(target);
        return false;
    }
    targets.push_back(target);
    return true;
}

void removeHooks(std::vector<void*>& targets) {
    for (auto target : targets)
        MH_DisableHook(target);
    for (auto target : targets)
        MH_RemoveHook(target);
    targets.clear();
}

LRESULT CALLBACK dummyProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

} // namespace

bool observeSwapChainCreation() {
    std::lock_guard lock(gInstallMutex);
    if (gObservingCreation)
        return true;
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        log::error("MH_Initialize failed: {}", int(status));
        return false;
    }
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;
    gCreationHookTargets.reserve(4); // no vector allocation after enabling a hook
    auto table = *reinterpret_cast<void***>(factory.Get());
    // Public COM slots only. Do not wait for a GPU device/window/native bridge:
    // the game can create its real chain while those slower steps are running.
    bool ok = hook(table[10], reinterpret_cast<void*>(&createSwapChainHook),
                   reinterpret_cast<void**>(&oCreateSwapChain), gCreationHookTargets) &&
              hook(table[15], reinterpret_cast<void*>(&createHwndHook),
                   reinterpret_cast<void**>(&oCreateHwnd), gCreationHookTargets) &&
              hook(table[16], reinterpret_cast<void*>(&createCoreHook),
                   reinterpret_cast<void**>(&oCreateCore), gCreationHookTargets) &&
              hook(table[24], reinterpret_cast<void*>(&createCompositionHook),
                   reinterpret_cast<void**>(&oCreateComposition), gCreationHookTargets);
    if (!ok) {
        removeHooks(gCreationHookTargets);
        log::error("DXGI creation observation refused");
        return false;
    }
    gObservingCreation = true;
    log::info("DXGI creation observation ready (before renderer installation)");
    return true;
}

bool install() {
    std::lock_guard lock(gInstallMutex);
    if (gInstalled)
        return true;
    if (!observeSwapChainCreation())
        log::warn("DXGI creation observation unavailable; installing Present without queue observation");
    gHookTargets.reserve(6);
    diagnostics::start();

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc   = dummyProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ModMenuBEDummy";
    RegisterClassExW(&wc);
    HWND dummy = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                                 wc.hInstance, nullptr);
    if (!dummy) {
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return false;
    }

    bool ok = false;
    IDXGIFactory4*      factory{};
    ID3D12Device*       device{};
    ID3D12CommandQueue* queue{};
    IDXGISwapChain1*    swapChain{};
    do {
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) break;
        const auto createDevice = systemD3D12CreateDevice();
        if (!createDevice) {
            log::error("System D3D12CreateDevice unavailable (Windows error {})", GetLastError());
            break;
        }
        if (FAILED(createDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
            break;
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue)))) break;
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width       = 64;
        sd.Height      = 64;
        sd.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc  = {1, 0};
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        if (FAILED(factory->CreateSwapChainForHwnd(queue, dummy, &sd, nullptr, nullptr, &swapChain))) break;

        void** scVtable    = *reinterpret_cast<void***>(swapChain);
        ok = hook(scVtable[kPresentIndex], reinterpret_cast<void*>(&presentHook), reinterpret_cast<void**>(&oPresent))
          && hook(scVtable[kResizeBuffersIndex], reinterpret_cast<void*>(&resizeBuffersHook),
                  reinterpret_cast<void**>(&oResizeBuffers));
        // Present1 / ResizeBuffers1 are optional extras.
        hook(scVtable[kPresent1Index], reinterpret_cast<void*>(&present1Hook), reinterpret_cast<void**>(&oPresent1));
        hook(scVtable[kResizeBuffers1Index], reinterpret_cast<void*>(&resizeBuffers1Hook),
             reinterpret_cast<void**>(&oResizeBuffers1));
    } while (false);
    safeRelease(swapChain);
    safeRelease(queue);
    safeRelease(device);
    safeRelease(factory);
    DestroyWindow(dummy);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    if (!ok) {
        // A renderer failure must not lose game creation events during retry.
        removeHooks(gHookTargets);
        log::error("could not hook DXGI Present");
        return false;
    }
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    hook(reinterpret_cast<void*>(GetProcAddress(user32, "SetCursorPos")), reinterpret_cast<void*>(&setCursorPosHook),
         reinterpret_cast<void**>(&oSetCursorPos));
    hook(reinterpret_cast<void*>(GetProcAddress(user32, "ClipCursor")), reinterpret_cast<void*>(&clipCursorHook),
         reinterpret_cast<void**>(&oClipCursor));
    gInstalled = true;
    log::info("DXGI hooks installed");
    return true;
}

void uninstall() {
    std::lock_guard lock(gInstallMutex);
    const bool wasInstalled = gInstalled.exchange(false);
    removeHooks(gCreationHookTargets);
    gObservingCreation = false;
    removeHooks(gHookTargets);
    if (!wasInstalled)
        return; // cancellation before admission has no graphics state
    {
        std::lock_guard windowLock(gWindowMutex);
        for (auto it = gWindowProcedures.begin(); it != gWindowProcedures.end();) {
            if (!IsWindow(it->first))
                it = gWindowProcedures.erase(it);
            else if (GetWindowLongPtrW(it->first, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(&wndProcHook)) {
                if (SetWindowLongPtrW(it->first, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(it->second)))
                    it = gWindowProcedures.erase(it);
                else
                    ++it;
            } else
                ++it; // retain a predecessor for any later plugin's live chain
        }
    }
    releaseGraphics();
    gInstalled = false;
}

bool installed() noexcept {
    return gInstalled.load();
}
HWND gameWindow() { return gWindow; }
bool graphicsFaulted() noexcept {
    return gGraphicsFailed.load();
}

ID3D11Device* device() { return gDevice; }

ID3D11ShaderResourceView* createTexture(void const* rgba, int width, int height) {
    if (!gDevice || !rgba || width <= 0 || height <= 0) return nullptr;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width            = UINT(width);
    desc.Height           = UINT(height);
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data{rgba, UINT(width * 4), 0};
    ID3D11Texture2D* tex{};
    if (FAILED(gDevice->CreateTexture2D(&desc, &data, &tex))) return nullptr;
    ID3D11ShaderResourceView* srv{};
    gDevice->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    return srv;
}

void releaseTexture(ID3D11ShaderResourceView* texture) {
    if (texture) texture->Release();
}

} // namespace modmenu::overlay
