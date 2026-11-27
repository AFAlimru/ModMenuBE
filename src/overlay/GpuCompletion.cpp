#include "overlay/GpuCompletion.h"
#include <cstdint>
namespace modmenu::overlay {
GpuCompletion::~GpuCompletion() {
    if (event_)
        CloseHandle(event_);
}
HRESULT GpuCompletion::initialize(ID3D11Device* device, ID3D11DeviceContext* context,
                                  ID3D12CommandQueue* queue) {
    if (context_)
        return E_UNEXPECTED;
    if (!device || !context)
        return E_INVALIDARG;
    Microsoft::WRL::ComPtr<ID3D11Device> contextDevice;
    Microsoft::WRL::ComPtr<IUnknown> expected, actual;
    context->GetDevice(&contextDevice);
    if (!contextDevice || FAILED(device->QueryInterface(IID_PPV_ARGS(&expected))) ||
        FAILED(contextDevice.As(&actual)) || actual != expected)
        return E_INVALIDARG;
    if (queue) {
        if (queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
            return E_INVALIDARG;
        auto hr = queue->GetDevice(IID_PPV_ARGS(&device12_));
        if (FAILED(hr))
            return hr;
        hr = device12_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
        if (FAILED(hr)) {
            device12_.Reset();
            return hr;
        }
        event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!event_) {
            fence_.Reset();
            device12_.Reset();
            return HRESULT_FROM_WIN32(GetLastError());
        }
        queue_ = queue;
    } else {
        D3D11_QUERY_DESC desc{D3D11_QUERY_EVENT, 0};
        auto hr = device->CreateQuery(&desc, &query_);
        if (FAILED(hr))
            return hr;
    }
    device11_ = device;
    context_ = context;
    return S_OK;
}
HRESULT GpuCompletion::wait(UINT timeoutMs) {
    if (timeoutMs > 2000)
        return E_INVALIDARG;
    if (!context_)
        return E_UNEXPECTED;
    if (!pending_) {
        pending_ = true;
        if (queue_) {
            context_->Flush();
            auto hr = queue_->Signal(fence_.Get(), ++value_);
            if (FAILED(hr))
                return hr;
            hr = fence_->SetEventOnCompletion(value_, event_);
            if (FAILED(hr))
                return hr;
        } else {
            context_->End(query_.Get());
            context_->Flush();
        }
    }
    const auto deadline = GetTickCount64() + timeoutMs;
    for (;;) {
        if (queue_) {
            const auto completed = fence_->GetCompletedValue();
            if (completed == UINT64_MAX)
                return DXGI_ERROR_DEVICE_REMOVED;
            if (completed >= value_) {
                pending_ = false;
                return S_OK;
            }
            auto removed = device12_->GetDeviceRemovedReason();
            if (FAILED(removed))
                return removed;
        } else {
            BOOL done = FALSE;
            auto hr = context_->GetData(query_.Get(), &done, sizeof(done), D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (FAILED(hr))
                return hr;
            if (hr == S_OK && done) {
                pending_ = false;
                return S_OK;
            }
            auto removed = device11_->GetDeviceRemovedReason();
            if (FAILED(removed))
                return removed;
        }
        const auto now = GetTickCount64();
        if (now >= deadline)
            return DXGI_ERROR_WAS_STILL_DRAWING;
        if (queue_) {
            const auto status = WaitForSingleObject(event_, DWORD(deadline - now));
            if (status == WAIT_FAILED)
                return HRESULT_FROM_WIN32(GetLastError());
            // Event delivery is advisory; read the authoritative fence again.
        } else
            Sleep(1);
    }
}
bool GpuCompletion::reset() {
    if (pending_)
        return false;
    query_.Reset();
    fence_.Reset();
    queue_.Reset();
    device12_.Reset();
    context_.Reset();
    device11_.Reset();
    if (event_) {
        CloseHandle(event_);
        event_ = nullptr;
    }
    value_ = 0;
    return true;
}
} // namespace modmenu::overlay
