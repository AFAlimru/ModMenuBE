// Separate test DLL: actual overlay/hooks with inert UI, no game bridge or providers.
#include <Windows.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <atomic>
#include <thread>
#include "ui/Ui.h"
#include "ui/Fonts.h"
#include "ui/LegacySettings.h"
#include "overlay/D3D12Runtime.h"

namespace {
std::atomic<bool> draw{}, correct{};
std::atomic<unsigned> calls{};
std::atomic<bool> failDeviceProbe{};
std::atomic<unsigned> deviceProbes{};
std::atomic<HWND> inputForeground{};
std::atomic<bool> exerciseHotkey{};
std::atomic<unsigned> hotkeyToggles{};
bool inputAccepted{}, simulateMouse{};
SHORT fixtureKeyState(int key) {
    return simulateMouse ? (key == VK_LBUTTON ? SHORT(0x8000) : 0) : GetAsyncKeyState(key);
}
HWND fixtureForegroundWindow() {
    return inputForeground.load();
}
IUnknown* expected{};
HRESULT WINAPI fixtureCreateOn12(IUnknown* device, UINT flags, D3D_FEATURE_LEVEL const* levels, UINT count,
                                 IUnknown* const* queues, UINT queueCount, UINT node, ID3D11Device** output,
                                 ID3D11DeviceContext** context, D3D_FEATURE_LEVEL* level) {
    IUnknown* identity{};
    if (queueCount == 1 && SUCCEEDED(queues[0]->QueryInterface(IID_PPV_ARGS(&identity)))) {
        correct = identity == expected;
        identity->Release();
    }
    ++calls;
    return D3D11On12CreateDevice(device, flags, levels, count, queues, queueCount, node, output, context,
                                 level);
}
} // namespace
namespace modmenu::overlay {
PFN_D3D12_CREATE_DEVICE fixtureSystemD3D12CreateDevice() noexcept {
    ++deviceProbes;
    return failDeviceProbe ? nullptr : systemD3D12CreateDevice();
}
} // namespace modmenu::overlay
#define D3D11On12CreateDevice fixtureCreateOn12
#define systemD3D12CreateDevice fixtureSystemD3D12CreateDevice
#define GetForegroundWindow fixtureForegroundWindow
#define GetAsyncKeyState fixtureKeyState
#include "../src/overlay/Overlay.cpp"
#undef GetAsyncKeyState
#undef GetForegroundWindow
#undef systemD3D12CreateDevice
#undef D3D11On12CreateDevice

namespace {
thread_local bool insideF11{};
std::atomic<unsigned> f11FlushCalls{};
using FlushFn = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*);
FlushFn originalFlush{};
HANDLE forwardEntered{}, forwardAcquired{};
HRESULT STDMETHODCALLTYPE probeOriginalPresent(IDXGISwapChain*, UINT, UINT) {
    SetEvent(forwardEntered);
    return WaitForSingleObject(forwardAcquired, 1000) == WAIT_OBJECT_0 ? S_OK : E_FAIL;
}
void STDMETHODCALLTYPE observeFlush(ID3D11DeviceContext* context) {
    if (insideF11 && context == modmenu::overlay::gContext)
        ++f11FlushCalls;
    originalFlush(context);
}
} // namespace

namespace modmenu::ui {
bool wantsFrame() {
    return draw;
}
bool prepareFrame(float, float) {
    return false;
}
void drawFrame() {
    ImGui::Begin("queue fixture");
    ImGui::TextUnformatted("GPU test");
    ImGui::End();
}
void onDeviceLost() {}
bool blocksGameInput() {
    return false;
}
bool acceptsUiInput() {
    return inputAccepted;
}
void notifyFocusLost() {}
void pollInputRelease() {}
bool handleWindowMessage(UINT msg, WPARAM key, LPARAM) {
    if (exerciseHotkey && msg == WM_KEYDOWN && key == VK_F8) {
        draw = !draw;
        ++hotkeyToggles;
        return true;
    }
    return false;
}
namespace legacy {
bool capturing() {
    return false;
}
bool captureKey(unsigned, bool) {
    return false;
}
} // namespace legacy
namespace fonts {
void note(std::string_view) {}
} // namespace fonts
} // namespace modmenu::ui
extern "C" __declspec(dllexport) bool InstallFixture() {
    return modmenu::overlay::install();
}
extern "C" __declspec(dllexport) bool MousePollingFixture(HWND hwnd) {
    auto context = ImGui::CreateContext();
    inputForeground = hwnd;
    modmenu::overlay::gWindow = hwnd;
    simulateMouse = true;
    inputAccepted = false;
    modmenu::overlay::pollMouseButtons();
    bool suspended = context->InputEventsQueue.empty();
    inputAccepted = true;
    modmenu::overlay::pollMouseButtons();
    bool clicked = context->InputEventsQueue.Size == 1;
    if (clicked) {
        auto const& event = context->InputEventsQueue[0];
        int expectedButton = GetSystemMetrics(SM_SWAPBUTTON) ? 1 : 0;
        clicked = event.Type == ImGuiInputEventType_MouseButton &&
                  event.MouseButton.Button == expectedButton && event.MouseButton.Down;
    }
    inputAccepted = false;
    simulateMouse = false;
    modmenu::overlay::gWindow = nullptr;
    ImGui::DestroyContext(context);
    return suspended && clicked;
}
extern "C" __declspec(dllexport) void ForegroundFixture(HWND hwnd) {
    inputForeground = hwnd;
    exerciseHotkey = true;
}
extern "C" __declspec(dllexport) HWND BoundWindowFixture() {
    return modmenu::overlay::gameWindow();
}
extern "C" __declspec(dllexport) unsigned HotkeyTogglesFixture() {
    return hotkeyToggles;
}
extern "C" __declspec(dllexport) void ObserveFixture() {
    modmenu::overlay::observeSwapChainCreation();
}
extern "C" __declspec(dllexport) void UninstallFixture() {
    modmenu::overlay::uninstall();
}
extern "C" __declspec(dllexport) unsigned DeviceProbesFixture() {
    return deviceProbes;
}
extern "C" __declspec(dllexport) bool FailInstallFixture() {
    failDeviceProbe = true;
    auto installed = modmenu::overlay::install();
    failDeviceProbe = false;
    return !installed;
}
namespace {
modmenu::overlay::CreateHwndFn blockedCreateHwnd{};
HRESULT STDMETHODCALLTYPE passCreateHwnd(IDXGIFactory2* factory, IUnknown* device, HWND hwnd,
                                         DXGI_SWAP_CHAIN_DESC1 const* desc,
                                         DXGI_SWAP_CHAIN_FULLSCREEN_DESC const* full, IDXGIOutput* output,
                                         IDXGISwapChain1** chain) {
    return blockedCreateHwnd(factory, device, hwnd, desc, full, output, chain);
}
} // namespace
extern "C" __declspec(dllexport) bool BlockObservationFixture() {
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
        return false;
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return false;
    auto target = (*reinterpret_cast<void***>(factory.Get()))[15];
    if (MH_CreateHook(target, reinterpret_cast<void*>(&passCreateHwnd),
                      reinterpret_cast<void**>(&blockedCreateHwnd)) != MH_OK)
        return false;
    if (MH_EnableHook(target) != MH_OK) {
        MH_RemoveHook(target);
        return false;
    }
    return true;
}
extern "C" __declspec(dllexport) void UnblockObservationFixture() {
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return;
    auto target = (*reinterpret_cast<void***>(factory.Get()))[15];
    MH_DisableHook(target);
    MH_RemoveHook(target);
}
extern "C" __declspec(dllexport) bool FailObserveFixture() {
    if (!BlockObservationFixture())
        return false;
    auto observed = modmenu::overlay::observeSwapChainCreation();
    UnblockObservationFixture();
    return !observed;
}
extern "C" __declspec(dllexport) void OpenFixture(IUnknown* queue) {
    if (expected)
        expected->Release();
    expected = nullptr;
    queue->QueryInterface(IID_PPV_ARGS(&expected));
    draw = true;
}
extern "C" __declspec(dllexport) unsigned QueueResult() {
    return calls ? (correct ? 1 : 2) : 0;
}
extern "C" __declspec(dllexport) void StopFixture() {
    draw = false;
}
extern "C" __declspec(dllexport) unsigned BufferCountFixture() {
    std::lock_guard lock(modmenu::overlay::gRenderMutex);
    return unsigned(modmenu::overlay::gBuffers.size());
}
extern "C" __declspec(dllexport) unsigned SendF11Fixture(HWND hwnd) {
    if (!originalFlush) {
        auto context = modmenu::overlay::gContext;
        if (!context)
            return UINT_MAX;
        auto table = *reinterpret_cast<void***>(context);
        if (!modmenu::overlay::hook(table[111], reinterpret_cast<void*>(&observeFlush),
                                    reinterpret_cast<void**>(&originalFlush)))
            return UINT_MAX;
    }
    insideF11 = true;
    SendMessageW(hwnd, WM_KEYDOWN, VK_F11, 0);
    insideF11 = false;
    return f11FlushCalls.load();
}
extern "C" __declspec(dllexport) void HoldGraphicsFixture(HANDLE entered, HANDLE release) {
    std::lock_guard lock(modmenu::overlay::gRenderMutex);
    SetEvent(entered);
    WaitForSingleObject(release, 1000);
}
extern "C" __declspec(dllexport) bool ProbePresentForwardingFixture(IDXGISwapChain* chain) {
    forwardEntered = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    forwardAcquired = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!forwardEntered || !forwardAcquired)
        return false;
    auto original = modmenu::overlay::oPresent;
    modmenu::overlay::oPresent = &probeOriginalPresent;
    std::thread observer([] {
        if (WaitForSingleObject(forwardEntered, 2000) != WAIT_OBJECT_0)
            return;
        std::lock_guard lock(modmenu::overlay::gRenderMutex);
        SetEvent(forwardAcquired);
    });
    auto hr = modmenu::overlay::presentHook(chain, 0, DXGI_PRESENT_TEST);
    observer.join();
    modmenu::overlay::oPresent = original;
    CloseHandle(forwardEntered);
    CloseHandle(forwardAcquired);
    return hr == S_OK;
}
extern "C" __declspec(dllexport) bool OwnsFixtureQueue(IDXGISwapChain* chain, IUnknown* queue) {
    auto owner = modmenu::overlay::swapchain::queue(chain);
    Microsoft::WRL::ComPtr<IUnknown> identity, found;
    return owner && queue && SUCCEEDED(queue->QueryInterface(IID_PPV_ARGS(&identity))) &&
           SUCCEEDED(owner.As(&found)) && found.Get() == identity.Get();
}
