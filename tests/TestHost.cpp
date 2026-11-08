// Development host: a D3D12 window that loads ModMenuBE.dll the way the game
// would, so the overlay can be checked without launching Minecraft.
//   TestHost.exe <path\to\mods\ModMenuBE\ModMenuBE.dll> [--open] [--seconds N]
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "../include/modmenu/ModMenuApi.h"

#include <cstdio>
#include <string>

namespace {

constexpr UINT kBuffers = 3;

LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::fwprintf(stderr, L"usage: TestHost <ModMenuBE.dll> [--open] [--seconds N]\n");
        return 2;
    }
    bool open    = false;
    int  seconds = 0;
    std::string selectMod;
    bool        openConfig = false;
    for (int i = 2; i < argc; ++i) {
        if (std::wstring(argv[i]) == L"--open") open = true;
        if (std::wstring(argv[i]) == L"--seconds" && i + 1 < argc) seconds = _wtoi(argv[++i]);
        if (std::wstring(argv[i]) == L"--mod" && i + 1 < argc) {
            std::wstring w = argv[++i];
            selectMod.assign(w.begin(), w.end());
        }
        if (std::wstring(argv[i]) == L"--config") openConfig = true;
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc   = wndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"ModMenuBETestHost";
    RegisterClassExW(&wc);
    RECT r{0, 0, 1600, 900};
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"ModMenuBE TestHost", WS_OVERLAPPEDWINDOW, 40, 40,
                                r.right - r.left, r.bottom - r.top, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);

    // Load the mod first, like a preload-native mod: before the device exists.
    if (!LoadLibraryW(argv[1])) {
        std::fwprintf(stderr, L"LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }

    IDXGIFactory4* factory{};
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    ID3D12Device* device{};
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) return 1;
    D3D12_COMMAND_QUEUE_DESC qd{};
    ID3D12CommandQueue* queue{};
    device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width       = 1600;
    sd.Height      = 900;
    sd.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc  = {1, 0};
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = kBuffers;
    sd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1* sc1{};
    if (FAILED(factory->CreateSwapChainForHwnd(queue, hwnd, &sd, nullptr, nullptr, &sc1))) return 1;
    IDXGISwapChain3* sc{};
    sc1->QueryInterface(IID_PPV_ARGS(&sc));

    D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kBuffers};
    ID3D12DescriptorHeap* heap{};
    device->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap));
    UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    ID3D12Resource* buffers[kBuffers]{};
    for (UINT i = 0; i < kBuffers; ++i) {
        sc->GetBuffer(i, IID_PPV_ARGS(&buffers[i]));
        D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += SIZE_T(i) * inc;
        device->CreateRenderTargetView(buffers[i], nullptr, h);
    }
    ID3D12CommandAllocator* alloc{};
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));
    ID3D12GraphicsCommandList* list{};
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list));
    list->Close();
    ID3D12Fence* fence{};
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    UINT64 fenceValue = 0;
    HANDLE event      = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    ULONGLONG start  = GetTickCount64();
    bool      opened = false;
    MSG       msg{};
    for (;;) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        ULONGLONG elapsed = GetTickCount64() - start;
        if (open && !opened && elapsed > 1500) {
            if (selectMod.empty()) {
                PostMessageW(hwnd, WM_KEYDOWN, VK_F8, 0x00420001);
            } else if (auto get = reinterpret_cast<ModMenu_GetApiFn>(
                           GetProcAddress(GetModuleHandleW(L"ModMenuBE.dll"), "ModMenu_GetApi"))) {
                if (ModMenuApi const* api = get(MODMENU_API_VERSION)) api->openMenu(selectMod.c_str(), openConfig);
            }
            opened = true;
        }
        if (seconds > 0 && elapsed > ULONGLONG(seconds) * 1000) return 0;

        UINT idx = sc->GetCurrentBackBufferIndex();
        alloc->Reset();
        list->Reset(alloc, nullptr);
        D3D12_RESOURCE_BARRIER b{};
        b.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource   = buffers[idx];
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        b.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
        list->ResourceBarrier(1, &b);
        D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += SIZE_T(idx) * inc;
        float sky[] = {0.45f, 0.65f, 0.95f, 1.0f};
        list->ClearRenderTargetView(h, sky, 0, nullptr);
        std::swap(b.Transition.StateBefore, b.Transition.StateAfter);
        list->ResourceBarrier(1, &b);
        list->Close();
        ID3D12CommandList* lists[] = {list};
        queue->ExecuteCommandLists(1, lists);
        sc->Present(1, 0);
        queue->Signal(fence, ++fenceValue);
        fence->SetEventOnCompletion(fenceValue, event);
        WaitForSingleObject(event, INFINITE);
    }
}
