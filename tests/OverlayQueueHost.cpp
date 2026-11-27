// Hidden, bounded GPU integration host. Never loads production ModMenuBE/Minecraft.
#include <Windows.h>
#include <d3d12.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdio>
#include <string>
#include <thread>
#include "overlay/D3D12Runtime.h"
using Microsoft::WRL::ComPtr;
namespace {
WNDPROC laterPredecessor{};
unsigned laterMessages{};
LRESULT CALLBACK laterProcedure(HWND hwnd, UINT msg, WPARAM key, LPARAM data) {
    if (msg == WM_APP + 50)
        ++laterMessages;
    return CallWindowProcW(laterPredecessor, hwnd, msg, key, data);
}
} // namespace
int wmain(int argc, wchar_t** argv) {
    if (argc < 2 || argc > 3)
        return 2;
    std::wstring mode = argc == 3 ? argv[2] : L"hwnd";
    auto module = LoadLibraryW(argv[1]);
    if (!module)
        return 2;
    auto install = reinterpret_cast<bool (*)()>(GetProcAddress(module, "InstallFixture"));
    auto observe = reinterpret_cast<void (*)()>(GetProcAddress(module, "ObserveFixture"));
    auto uninstall = reinterpret_cast<void (*)()>(GetProcAddress(module, "UninstallFixture"));
    auto failInstall = reinterpret_cast<bool (*)()>(GetProcAddress(module, "FailInstallFixture"));
    auto failObserve = reinterpret_cast<bool (*)()>(GetProcAddress(module, "FailObserveFixture"));
    auto deviceProbes = reinterpret_cast<unsigned (*)()>(GetProcAddress(module, "DeviceProbesFixture"));
    auto open = reinterpret_cast<void (*)(IUnknown*)>(GetProcAddress(module, "OpenFixture"));
    auto result = reinterpret_cast<unsigned (*)()>(GetProcAddress(module, "QueueResult"));
    auto stop = reinterpret_cast<void (*)()>(GetProcAddress(module, "StopFixture"));
    auto buffers = reinterpret_cast<unsigned (*)()>(GetProcAddress(module, "BufferCountFixture"));
    auto sendF11 = reinterpret_cast<unsigned (*)(HWND)>(GetProcAddress(module, "SendF11Fixture"));
    auto holdGraphics =
        reinterpret_cast<void (*)(HANDLE, HANDLE)>(GetProcAddress(module, "HoldGraphicsFixture"));
    auto probeForwarding =
        reinterpret_cast<bool (*)(IDXGISwapChain*)>(GetProcAddress(module, "ProbePresentForwardingFixture"));
    auto owns =
        reinterpret_cast<bool (*)(IDXGISwapChain*, IUnknown*)>(GetProcAddress(module, "OwnsFixtureQueue"));
    if (!install || !open || !result || !stop)
        return 2;
    bool observerFallback = mode == L"observer-fallback-d3d11" || mode == L"observer-fallback-d3d12";
    if (observerFallback) {
        auto block = reinterpret_cast<bool (*)()>(GetProcAddress(module, "BlockObservationFixture"));
        if (!block || !block())
            return 2;
        if (!install()) {
            std::puts("FAIL unavailable creation observation prevented Present installation");
            return 1;
        }
    }
    bool earlyObserver = mode == L"early-observer" || mode == L"observer-repeat" ||
                         mode == L"observer-abort" || mode == L"observer-reinstall" ||
                         mode == L"observer-retry" || mode == L"early-d3d11" || mode == L"observer-partial";
    if (earlyObserver) {
        if (!observe)
            return 2;
        if (mode == L"observer-partial" && (!failObserve || !failObserve())) {
            std::puts("FAIL partial observer failure was accepted");
            return 1;
        }
        observe();
        if (mode == L"observer-repeat")
            observe();
        if (!deviceProbes || deviceProbes() != 0) {
            std::puts("FAIL observation created a GPU probe");
            return 1;
        }
        if (mode == L"observer-abort" || mode == L"observer-reinstall") {
            if (!uninstall)
                return 2;
            uninstall();
            uninstall();
            if (mode == L"observer-reinstall")
                observe();
        }
        if (mode == L"observer-retry") {
            if (!failInstall || !failInstall()) {
                std::puts("FAIL unavailable renderer probe accepted");
                return 1;
            }
        }
    } else if (!observerFallback && mode != L"late" && !install())
        return 2;
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"ModMenuQueueHost";
    RegisterClassW(&wc);
    auto hwnd = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 128, 128, nullptr, nullptr,
                              wc.hInstance, nullptr);
    if (mode == L"mouse-polling-gate") {
        auto poll = reinterpret_cast<bool (*)(HWND)>(GetProcAddress(module, "MousePollingFixture"));
        bool ok = poll && poll(hwnd);
        std::puts(ok ? "mouse polling delivers swallowed clicks only after UI release gating" :
                       "FAIL mouse polling bypassed gating or lost a physical click");
        return ok ? 0 : 1;
    }
    if (mode == L"d3d11" || mode == L"f11-d3d11" || mode == L"early-d3d11" || mode == L"window-rebind" ||
        mode == L"window-destroy" || mode == L"observer-fallback-d3d11") {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width = 128;
        desc.BufferDesc.Height = 128;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.OutputWindow = hwnd;
        desc.Windowed = TRUE;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<ID3D11Device> d11;
        ComPtr<ID3D11DeviceContext> context;
        ComPtr<IDXGISwapChain> sc;
        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                                 D3D11_SDK_VERSION, &desc, &sc, &d11, nullptr, &context)))
            return 2;
        if (earlyObserver && !install())
            return 2;
        if (mode == L"window-rebind" || mode == L"window-destroy") {
            auto foreground = reinterpret_cast<void (*)(HWND)>(GetProcAddress(module, "ForegroundFixture"));
            auto bound = reinterpret_cast<HWND (*)()>(GetProcAddress(module, "BoundWindowFixture"));
            auto toggles = reinterpret_cast<unsigned (*)()>(GetProcAddress(module, "HotkeyTogglesFixture"));
            if (!foreground || !bound || !toggles)
                return 2;
            // A plugin's hidden probe presents before the real game window.
            foreground(hwnd);
            if (FAILED(sc->Present(0, 0)) || bound() != hwnd)
                return 2;
            if (mode == L"window-destroy") {
                if (!DestroyWindow(hwnd))
                    return 2;
                if (bound()) {
                    std::puts("FAIL destroyed window retained an input binding that can alias a reused HWND");
                    return 1;
                }
            } else {
                laterPredecessor = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&laterProcedure)));
                if (!laterPredecessor)
                    return 2;
            }
            auto game = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 192, 128, nullptr,
                                      nullptr, wc.hInstance, nullptr);
            if (!game)
                return 2;
            desc.OutputWindow = game;
            ComPtr<IDXGISwapChain> gameChain;
            if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                                                     D3D11_SDK_VERSION, &desc, &gameChain, &d11, nullptr,
                                                     &context)))
                return 2;
            foreground(game);
            if (FAILED(gameChain->Present(0, 0)))
                return 2;
            if (bound() != game) {
                std::puts("FAIL first auxiliary window permanently owns hotkey input");
                return 1;
            }
            SendMessageW(game, WM_KEYDOWN, VK_F8, 0);
            if (toggles() != 1 || FAILED(gameChain->Present(0, 0)) || !buffers || !buffers()) {
                std::puts("FAIL F8 on selected game window did not draw the overlay");
                return 1;
            }
            if (mode != L"window-destroy" && (FAILED(sc->Present(0, 0)) || bound() != game)) {
                std::puts("FAIL auxiliary Present stole the selected input window");
                return 1;
            }
            if (mode != L"window-destroy") {
                SendMessageW(hwnd, WM_APP + 50, 0, 0);
                SendMessageW(hwnd, WM_KEYDOWN, VK_F8, 0);
                if (GetWindowLongPtrW(hwnd, GWLP_WNDPROC) != reinterpret_cast<LONG_PTR>(&laterProcedure) ||
                    laterMessages != 1 || toggles() != 1) {
                    std::puts("FAIL rebinding damaged a later plugin's procedure or consumed its input");
                    return 1;
                }
            }
            std::puts("foreground game window replaced auxiliary input binding; F8 rendered; auxiliary "
                      "Present ignored");
            return 0;
        }
        open(d11.Get());
        for (int i = 0; i < 12; ++i) {
            if (FAILED(sc->Present(0, 0)))
                return 1;
            if (!buffers || !buffers()) {
                std::puts("FAIL D3D11 did not create overlay buffers");
                return 1;
            }
            if (mode == L"f11-d3d11") {
                if (!buffers || !buffers())
                    return 2;
                auto before = buffers();
                if (!sendF11 || sendF11(hwnd) != 0) {
                    std::puts("FAIL F11 callback flushed D3D11 context");
                    return 1;
                }
                if (buffers() != before) {
                    std::puts("FAIL F11 callback released D3D11 buffers");
                    return 1;
                }
                if (FAILED(sc->ResizeBuffers(0, 128 + (i % 2) * 16, 128, DXGI_FORMAT_UNKNOWN, 0)))
                    return 1;
            }
        }
        stop();
        std::printf("D3D11: 12 Presents; On12 calls=%u (expected 0)\n", result());
        return result() ? 1 : 0;
    }
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter> warp;
    ComPtr<ID3D12Device> device;
    auto create = modmenu::overlay::systemD3D12CreateDevice();
    if (!hwnd || !create || FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        return 2;
    if (mode == L"f11-hardware") {
        if (FAILED(factory->EnumAdapters(0, &warp)))
            return 2;
        DXGI_ADAPTER_DESC hardware{};
        warp->GetDesc(&hardware);
        std::printf("hardware adapter vendor=0x%04X device=0x%04X\n", hardware.VendorId, hardware.DeviceId);
    } else if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))))
        return 2;
    if (FAILED(create(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
        return 2;
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> early, owner;
    if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&early))) ||
        FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&owner))))
        return 2;
    // Execute on a real unrelated DIRECT queue before creating the displayed chain.
    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> list;
    if (FAILED(device->CreateCommandAllocator(qd.Type, IID_PPV_ARGS(&alloc))) ||
        FAILED(device->CreateCommandList(0, qd.Type, alloc.Get(), nullptr, IID_PPV_ARGS(&list))))
        return 2;
    list->Close();
    ID3D12CommandList* lists[] = {list.Get()};
    early->ExecuteCommandLists(1, lists);
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = 128;
    sd.Height = 128;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferCount = 2;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> chain;
    if (mode == L"legacy") {
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferDesc.Width = 128;
        desc.BufferDesc.Height = 128;
        desc.BufferDesc.Format = sd.Format;
        desc.SampleDesc = sd.SampleDesc;
        desc.BufferUsage = sd.BufferUsage;
        desc.BufferCount = sd.BufferCount;
        desc.OutputWindow = hwnd;
        desc.Windowed = TRUE;
        desc.SwapEffect = sd.SwapEffect;
        ComPtr<IDXGISwapChain> legacy;
        if (FAILED(factory->CreateSwapChain(owner.Get(), &desc, &legacy)) || FAILED(legacy.As(&chain)))
            return 2;
    } else if (mode == L"composition") {
        sd.Scaling = DXGI_SCALING_STRETCH;
        if (FAILED(factory->CreateSwapChainForComposition(owner.Get(), &sd, nullptr, &chain)))
            return 2;
    } else {
        if (FAILED(factory->CreateSwapChainForHwnd(owner.Get(), hwnd, &sd, nullptr, nullptr, &chain)))
            return 2;
    }
    if (earlyObserver && !install())
        return 2;
    if (mode == L"late" || mode == L"observer-abort" || mode == L"observer-fallback-d3d12") {
        if (!install())
            return 2;
        open(owner.Get());
        auto hr = chain->Present(0, 0);
        stop();
        std::printf("unobserved swapchain: Present=0x%08X; On12 calls=%u (expected 0)\n", unsigned(hr),
                    result());
        return SUCCEEDED(hr) && result() == 0 ? 0 : 1;
    }
    if (!owns || !owns(chain.Get(), owner.Get())) {
        std::puts("FAIL exact creation queue not recorded");
        return 1;
    }
    if (mode == L"composition") {
        std::puts("composition creation queue recorded; no HWND render path");
        return 0;
    }
    if (mode == L"resize" || mode == L"multi-resize" || mode == L"live-resize") {
        if (mode == L"live-resize") {
            open(owner.Get());
            if (FAILED(chain->Present(0, 0)))
                return 2;
            ComPtr<ID3D12Fence> warmFence;
            auto warm = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (!warm || FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&warmFence))) ||
                FAILED(owner->Signal(warmFence.Get(), 1)) ||
                FAILED(warmFence->SetEventOnCompletion(1, warm)) ||
                WaitForSingleObject(warm, 3000) != WAIT_OBJECT_0)
                return 2;
            CloseHandle(warm);
        }
        ComPtr<IDXGISwapChain3> sc3;
        if (FAILED(chain.As(&sc3)))
            return 2;
        IUnknown* nextQueues[] = {early.Get(), mode == L"multi-resize" ? owner.Get() : early.Get()};
        UINT masks[] = {1, 1};
        auto hr = sc3->ResizeBuffers1(2, 128, 128, sd.Format, 0, masks, nextQueues);
        if (FAILED(hr))
            return 2;
        if (mode == L"multi-resize") {
            auto known = owns(chain.Get(), early.Get()) || owns(chain.Get(), owner.Get());
            std::printf("multi-queue resize admitted=%u (expected 0)\n", unsigned(known));
            return known ? 1 : 0;
        }
        if (!owns(chain.Get(), early.Get())) {
            std::puts("FAIL successful ResizeBuffers1 queue change not recorded");
            return 1;
        }
        owner = early;
    }
    if (mode == L"failed-resize") {
        // A failed API operation must not associate a queue it did not accept.
        ComPtr<IDXGISwapChain3> sc3;
        ComPtr<ID3D12Resource> held;
        if (FAILED(chain.As(&sc3)) || FAILED(chain->GetBuffer(0, IID_PPV_ARGS(&held))))
            return 2;
        IUnknown* nextQueues[] = {early.Get(), early.Get()};
        UINT masks[] = {1, 1};
        if (SUCCEEDED(sc3->ResizeBuffers1(2, 128, 128, sd.Format, 0, masks, nextQueues)))
            return 2;
        if (!owns(chain.Get(), owner.Get())) {
            std::puts("FAIL failed ResizeBuffers1 changed owner");
            return 1;
        }
    }
    if (mode == L"failed-create") {
        auto invalid = sd;
        invalid.BufferCount = 0;
        ComPtr<IDXGISwapChain1> rejected;
        if (SUCCEEDED(
                factory->CreateSwapChainForHwnd(early.Get(), hwnd, &invalid, nullptr, nullptr, &rejected)))
            return 2;
        if (!owns(chain.Get(), owner.Get()))
            return 1;
    }
    open(owner.Get());
    auto hr = chain->Present(0, 0);
    if (mode == L"present-forwarding") {
        if (!probeForwarding || !probeForwarding(chain.Get())) {
            std::puts("FAIL render mutex held across original Present forwarding");
            return 1;
        }
        std::puts("original Present forwarding runs without the overlay resource mutex");
    }
    if (mode == L"f11-empty") {
        if (FAILED(hr) || !buffers || !sendF11)
            return 2;
        stop();
        if (FAILED(chain->Present(0, 0)) || buffers() != 0)
            return 2;
        if (sendF11(hwnd) != 0) {
            std::puts("FAIL empty F11 callback still flushed context");
            return 1;
        }
        if (FAILED(chain->Present(0, 0)) || buffers() != 0)
            return 1;
        open(owner.Get());
        hr = chain->Present(0, 0);
        if (FAILED(hr) || !buffers())
            return 1;
        std::puts("empty-buffer F11 performed no Flush; reopening rendered normally");
    }
    if (mode == L"unrelated-resize") {
        if (FAILED(hr) || !buffers || !buffers())
            return 2;
        auto before = buffers();
        auto otherWindow = CreateWindowW(wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 128, 128, nullptr,
                                         nullptr, wc.hInstance, nullptr);
        ComPtr<IDXGISwapChain1> other;
        if (!otherWindow ||
            FAILED(
                factory->CreateSwapChainForHwnd(owner.Get(), otherWindow, &sd, nullptr, nullptr, &other)) ||
            FAILED(other->ResizeBuffers(0, 144, 128, DXGI_FORMAT_UNKNOWN, 0)))
            return 2;
        if (buffers() != before) {
            std::puts("FAIL unrelated swapchain resize cleaned active overlay buffers");
            return 1;
        }
        std::puts("unrelated resize preserved active overlay buffers");
    }
    if (mode == L"cleanup-timeout") {
        if (FAILED(hr) || !buffers || !buffers())
            return 2;
        auto before = buffers();
        ComPtr<ID3D12Fence> gate;
        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate))) ||
            FAILED(owner->Wait(gate.Get(), 1)))
            return 2;
        auto began = GetTickCount64();
        auto resize = chain->ResizeBuffers(0, 144, 128, DXGI_FORMAT_UNKNOWN, 0);
        auto elapsed = GetTickCount64() - began;
        gate->Signal(1); // Always unblock this owned GPU fixture before checking.
        DXGI_SWAP_CHAIN_DESC preserved{};
        chain->GetDesc(&preserved);
        if (resize != DXGI_ERROR_WAS_STILL_DRAWING || elapsed > 3000 || buffers() != before ||
            preserved.BufferDesc.Width != 128) {
            std::printf("FAIL cleanup timeout hr=0x%08X elapsed=%llu retained=%u\n", unsigned(resize),
                        elapsed, buffers());
            return 1;
        }
        if (FAILED(chain->ResizeBuffers(0, 144, 128, DXGI_FORMAT_UNKNOWN, 0)) || buffers() != 0)
            return 1;
        std::puts(
            "GPU timeout retained buffers and refused resize; after unblocking, cleanup/resize completed");
    }
    if (mode == L"f11-lock") {
        if (FAILED(hr) || !sendF11 || !holdGraphics)
            return 2;
        auto entered = CreateEventW(nullptr, FALSE, FALSE, nullptr),
             release = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!entered || !release)
            return 2;
        std::thread ownerThread([&] { holdGraphics(entered, release); });
        if (WaitForSingleObject(entered, 2000) != WAIT_OBJECT_0) {
            SetEvent(release);
            ownerThread.join();
            return 2;
        }
        auto began = GetTickCount64();
        auto flushes = sendF11(hwnd);
        auto elapsed = GetTickCount64() - began;
        SetEvent(release);
        ownerThread.join();
        CloseHandle(entered);
        CloseHandle(release);
        if (flushes || elapsed >= 500) {
            std::printf("FAIL F11 waited for render mutex: %llu ms\n", elapsed);
            return 1;
        }
        std::puts("F11 returned while another thread held the render mutex; no context Flush");
        hr = chain->Present(0, 0);
    }
    if (mode == L"f11-resize" || mode == L"f11-hardware") {
        for (int i = 0; i < 24; ++i) {
            if (FAILED(hr) || !buffers || !buffers())
                return 2;
            auto before = buffers();
            if (!sendF11 || sendF11(hwnd) != 0) {
                std::puts("FAIL F11 callback flushed D3D12 context");
                return 1;
            }
            if (buffers() != before) {
                std::puts("FAIL F11 callback released D3D12 buffers");
                return 1;
            }
            // Deliberately no external fence wait: the production resize hook
            // must complete its own outstanding overlay GPU work before release.
            if (FAILED(chain->ResizeBuffers(0, 128 + (i % 2) * 16, 128, DXGI_FORMAT_UNKNOWN, 0)))
                return 1;
            hr = chain->Present(0, 0);
        }
        std::puts("24 F11/resize cycles preserved callback boundary and rendered again");
    }
    ComPtr<ID3D12Fence> fence;
    auto event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event || FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
        return 2;
    for (UINT64 frame = 1; frame <= 12; ++frame) {
        if (FAILED(hr) || FAILED(owner->Signal(fence.Get(), frame)) ||
            FAILED(fence->SetEventOnCompletion(frame, event)) ||
            WaitForSingleObject(event, 3000) != WAIT_OBJECT_0 || FAILED(device->GetDeviceRemovedReason())) {
            std::puts("FAIL actual GPU progress/device health");
            CloseHandle(event);
            return 1;
        }
        hr = chain->Present(0, 0);
    }
    CloseHandle(event);
    auto selected = result();
    stop();
    std::printf("actual overlay: 12 GPU completions, Present=0x%08X; selected=%u (1=creation queue, 2=wrong "
                "queue, 0=no draw)\n",
                unsigned(hr), selected);
    return selected == 1 ? 0 : 1;
}
