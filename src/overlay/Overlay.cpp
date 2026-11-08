#include "overlay/Overlay.h"

#include "core/Log.h"
#include "ui/MenuControl.h"
#include "ui/Ui.h"

#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <atomic>
#include <mutex>
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
using ExecuteCommandListsFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using SetCursorPosFn        = BOOL(WINAPI*)(int, int);
using ClipCursorFn          = BOOL(WINAPI*)(RECT const*);

constexpr size_t kPresentIndex             = 8;
constexpr size_t kResizeBuffersIndex       = 13;
constexpr size_t kPresent1Index            = 22;
constexpr size_t kResizeBuffers1Index      = 39;
constexpr size_t kExecuteCommandListsIndex = 10;

PresentFn             oPresent{};
Present1Fn            oPresent1{};
ResizeBuffersFn       oResizeBuffers{};
ResizeBuffers1Fn      oResizeBuffers1{};
ExecuteCommandListsFn oExecuteCommandLists{};
SetCursorPosFn        oSetCursorPos{};
ClipCursorFn          oClipCursor{};

std::vector<void*> gHookTargets;
std::atomic<bool>  gInstalled{false};
std::recursive_mutex gRenderMutex; // frame rendering vs. teardown (resize / F11 / uninstall)

// Graphics state, guarded by gRenderMutex.
IDXGISwapChain*      gSwapChain{};
ID3D11Device*        gDevice{};
ID3D11DeviceContext* gContext{};
ID3D11On12Device*    gDevice11On12{};
ID3D12CommandQueue*  gCapturedQueue{};   // from ExecuteCommandLists (fallback)
bool                 gIsD3D12 = false;
bool                 gImGuiReady = false;
bool                 gGraphicsFailed = false;

struct BackBuffer {
    ID3D11Resource*         resource{}; // wrapped (D3D12) or plain (D3D11)
    ID3D11RenderTargetView* rtv{};
};
std::vector<BackBuffer> gBuffers;

HWND    gWindow{};
WNDPROC gOriginalWndProc{};

// ImGui is not thread-safe: the window thread queues messages, the render thread feeds them to ImGui.
struct QueuedMessage {
    UINT   msg;
    WPARAM wParam;
    LPARAM lParam;
};
std::mutex                 gMessageMutex;
std::vector<QueuedMessage> gMessages;

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
    for (auto const& m : pending) ImGui_ImplWin32_WndProcHandler(gWindow, m.msg, m.wParam, m.lParam);
}

void releaseBackBuffers() {
    if (gContext) {
        gContext->OMSetRenderTargets(0, nullptr, nullptr);
    }
    for (auto& b : gBuffers) {
        safeRelease(b.rtv);
        safeRelease(b.resource);
    }
    gBuffers.clear();
    if (gContext) {
        gContext->ClearState();
        gContext->Flush();
    }
}

void releaseGraphics() {
    std::lock_guard lock(gRenderMutex);
    releaseBackBuffers();
    if (gImGuiReady) {
        ui::onDeviceLost();
        ImGui_ImplDX11_Shutdown();
        gImGuiReady = false;
    }
    safeRelease(gDevice11On12);
    safeRelease(gContext);
    safeRelease(gDevice);
    gSwapChain = nullptr;
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
        // A D3D12 swap chain was created from a command queue; DXGI hands it back.
        ID3D12CommandQueue* queue{};
        if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&queue)))) {
            queue = gCapturedQueue;
            if (queue) queue->AddRef();
        }
        if (!queue) {
            device12->Release();
            return false; // wait for ExecuteCommandLists to capture one
        }
        IUnknown* queues[] = {queue};
        HRESULT   hr = D3D11On12CreateDevice(device12, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, queues, 1, 0,
                                             &gDevice, &gContext, nullptr);
        queue->Release();
        device12->Release();
        if (FAILED(hr) || FAILED(gDevice->QueryInterface(IID_PPV_ARGS(&gDevice11On12)))) {
            log::error("D3D11On12CreateDevice failed: 0x{:08X}", unsigned(hr));
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
    if (msg == WM_KEYDOWN && wParam == VK_F11) {
        // Fullscreen toggles can rebuild the swap chain outside ResizeBuffers;
        // drop every buffer reference first.
        std::lock_guard lock(gRenderMutex);
        releaseBackBuffers();
    }
    if (ui::handleWindowMessage(msg, wParam, lParam)) return 0;

    if (ui::isOpen() && msg != WM_SETCURSOR && msg != WM_INPUT) queueMessage(msg, wParam, lParam);
    if (ui::blocksGameInput()) {
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
            return msg == WM_IME_COMPOSITION || msg == WM_IME_STARTCOMPOSITION || msg == WM_IME_ENDCOMPOSITION
                     ? DefWindowProcW(hwnd, msg, wParam, lParam)
                     : 0;
        default: break;
        }
    }
    return CallWindowProcW(gOriginalWndProc, hwnd, msg, wParam, lParam);
}

void attachWindow(HWND hwnd) {
    if (!hwnd || gWindow == hwnd) return;
    gWindow          = hwnd;
    gOriginalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&wndProcHook))
    );
    if (!ImGui::GetCurrentContext()) {
        ImGui::CreateContext();
        ImGuiIO& io    = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }
    ImGui_ImplWin32_Init(hwnd);
    log::info("attached to game window {:p}", static_cast<void*>(hwnd));
}

void renderFrame(IDXGISwapChain* swapChain) {
    if (!gWindow) {
        // Attach early so the hotkey works before the menu was ever drawn.
        DXGI_SWAP_CHAIN_DESC desc{};
        if (SUCCEEDED(swapChain->GetDesc(&desc)) && IsWindow(desc.OutputWindow)) attachWindow(desc.OutputWindow);
    }
    if (!ui::wantsFrame()) {
        {
            std::lock_guard lock(gMessageMutex);
            gMessages.clear();
        }
        // Hold no back buffer references while the menu is closed.
        if (!gBuffers.empty()) {
            std::lock_guard lock(gRenderMutex);
            releaseBackBuffers();
        }
        return;
    }
    std::lock_guard lock(gRenderMutex);
    if (gGraphicsFailed) return;
    if (gSwapChain && gSwapChain != swapChain) releaseGraphics();
    if (!gWindow) return;
    if (!gImGuiReady && !initGraphics(swapChain)) return;
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

    if (ui::prepareFrame(width, height)) ImGui_ImplDX11_InvalidateDeviceObjects();
    ImGui_ImplDX11_NewFrame();
    drainMessages();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ui::drawFrame();
    ImGui::Render();

    auto& buffer = gBuffers[index];
    if (gIsD3D12) gDevice11On12->AcquireWrappedResources(&buffer.resource, 1);
    gContext->OMSetRenderTargets(1, &buffer.rtv, nullptr);
    D3D11_VIEWPORT vp{0, 0, width, height, 0, 1};
    gContext->RSSetViewports(1, &vp);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    if (gIsD3D12) gDevice11On12->ReleaseWrappedResources(&buffer.resource, 1);
    gContext->Flush();
}

HRESULT STDMETHODCALLTYPE presentHook(IDXGISwapChain* swapChain, UINT sync, UINT flags) {
    if (!(flags & DXGI_PRESENT_TEST)) renderFrame(swapChain);
    return oPresent(swapChain, sync, flags);
}

HRESULT STDMETHODCALLTYPE present1Hook(IDXGISwapChain1* swapChain, UINT sync, UINT flags,
                                       DXGI_PRESENT_PARAMETERS const* params) {
    if (!(flags & DXGI_PRESENT_TEST)) renderFrame(swapChain);
    return oPresent1(swapChain, sync, flags, params);
}

HRESULT STDMETHODCALLTYPE resizeBuffersHook(IDXGISwapChain* swapChain, UINT count, UINT w, UINT h, DXGI_FORMAT fmt,
                                            UINT flags) {
    {
        std::lock_guard lock(gRenderMutex);
        releaseBackBuffers();
    }
    return oResizeBuffers(swapChain, count, w, h, fmt, flags);
}

HRESULT STDMETHODCALLTYPE resizeBuffers1Hook(IDXGISwapChain3* swapChain, UINT count, UINT w, UINT h, DXGI_FORMAT fmt,
                                             UINT flags, UINT const* mask, IUnknown* const* queues) {
    {
        std::lock_guard lock(gRenderMutex);
        releaseBackBuffers();
    }
    return oResizeBuffers1(swapChain, count, w, h, fmt, flags, mask, queues);
}

void STDMETHODCALLTYPE executeCommandListsHook(ID3D12CommandQueue* queue, UINT count, ID3D12CommandList* const* lists) {
    if (!gCapturedQueue && queue->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
        queue->AddRef();
        gCapturedQueue = queue;
    }
    oExecuteCommandLists(queue, count, lists);
}

// While the menu holds the mouse, the game must not re-center or confine it.
BOOL WINAPI setCursorPosHook(int x, int y) {
    if (ui::blocksGameInput()) return TRUE;
    return oSetCursorPos(x, y);
}

BOOL WINAPI clipCursorHook(RECT const* rect) {
    if (ui::blocksGameInput()) return oClipCursor(nullptr);
    return oClipCursor(rect);
}

bool hook(void* target, void* detour, void** original) {
    if (!target) return false;
    if (MH_CreateHook(target, detour, original) != MH_OK) return false;
    if (MH_EnableHook(target) != MH_OK) {
        MH_RemoveHook(target);
        return false;
    }
    gHookTargets.push_back(target);
    return true;
}

LRESULT CALLBACK dummyProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

} // namespace

bool install() {
    if (gInstalled) return true;
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        log::error("MH_Initialize failed: {}", int(status));
        return false;
    }

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc   = dummyProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ModMenuBEDummy";
    RegisterClassExW(&wc);
    HWND dummy = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr,
                                 wc.hInstance, nullptr);
    if (!dummy) return false;

    bool ok = false;
    IDXGIFactory4*      factory{};
    ID3D12Device*       device{};
    ID3D12CommandQueue* queue{};
    IDXGISwapChain1*    swapChain{};
    do {
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) break;
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) break;
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
        void** queueVtable = *reinterpret_cast<void***>(queue);
        ok = hook(scVtable[kPresentIndex], reinterpret_cast<void*>(&presentHook), reinterpret_cast<void**>(&oPresent))
          && hook(scVtable[kResizeBuffersIndex], reinterpret_cast<void*>(&resizeBuffersHook),
                  reinterpret_cast<void**>(&oResizeBuffers));
        // Present1 / ResizeBuffers1 are optional extras.
        hook(scVtable[kPresent1Index], reinterpret_cast<void*>(&present1Hook), reinterpret_cast<void**>(&oPresent1));
        hook(scVtable[kResizeBuffers1Index], reinterpret_cast<void*>(&resizeBuffers1Hook),
             reinterpret_cast<void**>(&oResizeBuffers1));
        hook(queueVtable[kExecuteCommandListsIndex], reinterpret_cast<void*>(&executeCommandListsHook),
             reinterpret_cast<void**>(&oExecuteCommandLists));
    } while (false);
    safeRelease(swapChain);
    safeRelease(queue);
    safeRelease(device);
    safeRelease(factory);
    DestroyWindow(dummy);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    if (!ok) {
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
    if (!gInstalled) return;
    for (void* t : gHookTargets) {
        MH_DisableHook(t);
        MH_RemoveHook(t);
    }
    gHookTargets.clear();
    if (gWindow && gOriginalWndProc) {
        SetWindowLongPtrW(gWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(gOriginalWndProc));
    }
    releaseGraphics();
    gInstalled = false;
}

HWND gameWindow() { return gWindow; }

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
