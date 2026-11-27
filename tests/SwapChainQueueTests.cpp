// Actual D3D12/DXGI COM objects in a hidden test window; no Minecraft or plugin DLL.
#include "overlay/SwapChainQueue.h"
#include "overlay/D3D12Runtime.h"
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <iostream>

using Microsoft::WRL::ComPtr;
using namespace modmenu::overlay;
int main() {
    int checks = 0, failures = 0;
    auto check = [&](bool ok, char const* why) {
        ++checks;
        if (!ok) {
            ++failures;
            std::cerr << "FAIL " << why << '\n';
        }
    };
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ModMenuQueueFixture";
    RegisterClassW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 128, 128, nullptr, nullptr,
                              wc.hInstance, nullptr);
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter> warp;
    ComPtr<ID3D12Device> device;
    auto create = systemD3D12CreateDevice();
    if (!hwnd || !create || FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) ||
        FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) ||
        FAILED(create(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
        return 2;
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> early, owner;
    if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&early))) ||
        FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&owner))))
        return 2;
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = 128;
    sd.Height = 128;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferCount = 2;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> sc;
    if (FAILED(factory->CreateSwapChainForHwnd(owner.Get(), hwnd, &sd, nullptr, nullptr, &sc)))
        return 2;
    // Same device and same DIRECT type do not establish swapchain ownership.
    check(early.Get() != owner.Get(), "fixture has two distinct DIRECT queues on the same device");
    ComPtr<ID3D12CommandQueue> queried;
    const auto getHr = sc->GetDevice(IID_PPV_ARGS(&queried));
    std::cout << "GetDevice(command queue) HRESULT=0x" << std::hex << unsigned(getHr) << std::dec << '\n';
    // An unknown chain is never allowed to borrow the first queue seen.
    check(!swapchain::queue(sc.Get()), "unobserved swapchain has no established queue");
    check(swapchain::bind(sc.Get(), owner.Get()), "successful creation binds its exact pDevice queue");
    auto bound = swapchain::queue(sc.Get());
    check(bound.Get() == owner.Get(),
          "selected queue is the creation queue, regardless of earlier queue activity");
    check(bound.Get() != early.Get(), "same-device DIRECT queue cannot replace the observed owner");
    check(!swapchain::bind(sc.Get(), device.Get()), "a device is not a queue association");
    check(swapchain::queue(sc.Get()).Get() == owner.Get(), "failed association preserves established owner");
    check(!swapchain::bind(nullptr, owner.Get()) && !swapchain::bind(sc.Get(), nullptr),
          "null inputs rejected");
    check(!swapchain::queue(nullptr), "null swapchain rejected");
    qd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    ComPtr<ID3D12CommandQueue> copy;
    if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&copy))))
        return 2;
    check(!swapchain::bind(sc.Get(), copy.Get()), "COPY queue rejected");
    ComPtr<ID3D12Device> foreign;
    if (FAILED(create(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&foreign))))
        return 2;
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> foreignQueue;
    if (FAILED(foreign->CreateCommandQueue(&qd, IID_PPV_ARGS(&foreignQueue))))
        return 2;
    check(!swapchain::bind(sc.Get(), foreignQueue.Get()), "queue from another device rejected");
    check(swapchain::queue(sc.Get()).Get() == owner.Get(), "invalid replacement cannot discard known owner");
    check(SUCCEEDED(sc->ResizeBuffers(0, 128, 128, DXGI_FORMAT_UNKNOWN, 0)),
          "real swapchain resize succeeds");
    check(swapchain::queue(sc.Get()).Get() == owner.Get(), "same-queue resize retains association");
    bound.Reset();
    sc.Reset();
    // New objects must not inherit stale map entries, even if pointer addresses recycle.
    if (FAILED(factory->CreateSwapChainForHwnd(early.Get(), hwnd, &sd, nullptr, nullptr, &sc)))
        return 2;
    check(!swapchain::queue(sc.Get()), "destroyed swapchain association never leaks to a replacement");
    check(swapchain::bind(sc.Get(), early.Get()) && swapchain::queue(sc.Get()).Get() == early.Get(),
          "replacement can bind its own queue");
    sc.Reset();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    std::cout << checks << " checks / " << failures << " failures\n";
    return failures ? 1 : 0;
}
