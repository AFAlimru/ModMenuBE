#include "overlay/GpuCompletion.h"
#include "overlay/D3D12Runtime.h"
#include <d3d11on12.h>
#include <dxgi1_4.h>
#include <cstdio>
using Microsoft::WRL::ComPtr;
using modmenu::overlay::GpuCompletion;
int main() {
    unsigned checks = 0, failures = 0;
    auto check = [&](bool ok, char const* name) {
        ++checks;
        if (!ok) {
            ++failures;
            std::printf("FAIL %s\n", name);
        }
    };
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter> warp;
    ComPtr<ID3D12Device> device;
    auto create = modmenu::overlay::systemD3D12CreateDevice();
    if (!create || FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) ||
        FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))) ||
        FAILED(create(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
        return 2;
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue))))
        return 2;
    IUnknown* queues[] = {queue.Get()};
    ComPtr<ID3D11Device> d11;
    ComPtr<ID3D11DeviceContext> context;
    if (FAILED(D3D11On12CreateDevice(device.Get(), 0, nullptr, 0, queues, 1, 0, &d11, &context, nullptr)))
        return 2;
    GpuCompletion completion;
    auto initialized = completion.initialize(d11.Get(), context.Get(), queue.Get());
    check(SUCCEEDED(initialized), "completion initializes on actual On12 creation queue");
    if (SUCCEEDED(initialized)) {
        check(completion.wait() == S_OK, "normal queue marker completes");
        check(completion.wait(INFINITE) == E_INVALIDARG, "infinite waits rejected");
        ComPtr<ID3D12Fence> gate;
        if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gate))) ||
            FAILED(queue->Wait(gate.Get(), 1)))
            return 2;
        auto begin = GetTickCount64();
        auto hr = completion.wait(20);
        check(hr == DXGI_ERROR_WAS_STILL_DRAWING,
              "blocked GPU queue times out instead of claiming completion");
        check(GetTickCount64() - begin < 1000, "blocked queue wait is bounded");
        check(!completion.reset(), "timeout retains completion ownership");
        check(completion.initialize(d11.Get(), context.Get(), queue.Get()) == E_UNEXPECTED,
              "pending owner cannot be replaced");
        gate->Signal(1);
        check(completion.wait() == S_OK, "same marker recovers after real GPU progress");
        check(completion.reset(), "completed owner may be reset");
        check(completion.wait() == E_UNEXPECTED, "uninitialized completion cannot authorize release");
        GpuCompletion invalid;
        check(invalid.initialize(nullptr, context.Get()) == E_INVALIDARG, "null device refused");
        check(invalid.initialize(d11.Get(), nullptr) == E_INVALIDARG, "null context refused");
        ComPtr<ID3D12CommandQueue> copy;
        qd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        if (FAILED(device->CreateCommandQueue(&qd, IID_PPV_ARGS(&copy))))
            return 2;
        check(FAILED(invalid.initialize(d11.Get(), context.Get(), copy.Get())), "non-DIRECT queue refused");
    }
    ComPtr<ID3D11Device> plain;
    ComPtr<ID3D11DeviceContext> plainContext;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION,
                                 &plain, nullptr, &plainContext)))
        return 2;
    GpuCompletion mismatched;
    check(mismatched.initialize(plain.Get(), context.Get()) == E_INVALIDARG,
          "context from a different D3D11 device refused");
    GpuCompletion plainCompletion;
    auto plainReady = plainCompletion.initialize(plain.Get(), plainContext.Get());
    check(SUCCEEDED(plainReady), "plain D3D11 completion initializes");
    if (SUCCEEDED(plainReady)) {
        check(plainCompletion.wait() == S_OK, "D3D11 event query confirms completion");
        check(plainCompletion.wait() == S_OK, "D3D11 event query can be reused");
        check(plainCompletion.reset(), "D3D11 completed query may be reset");
    }
    std::printf("%u checks / %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
