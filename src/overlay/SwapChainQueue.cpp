#include "overlay/SwapChainQueue.h"
namespace modmenu::overlay::swapchain {
using Microsoft::WRL::ComPtr;
namespace {
// DXGI owns the interface reference. Destruction also destroys the association;
// no global raw-pointer map can leak a queue to a recycled swapchain address.
constexpr GUID ownerKey{0x430d3063, 0x13e3, 0x4acf, {0xab, 0xf4, 0x69, 0x59, 0x23, 0x78, 0x73, 0x19}};
bool compatible(IDXGISwapChain* chain, ID3D12CommandQueue* candidate) noexcept {
    if (!chain || !candidate || candidate->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT)
        return false;
    ComPtr<IUnknown> chainDevice, queueDevice;
    return SUCCEEDED(chain->GetDevice(IID_PPV_ARGS(&chainDevice))) &&
           SUCCEEDED(candidate->GetDevice(IID_PPV_ARGS(&queueDevice))) &&
           chainDevice.Get() == queueDevice.Get();
}
} // namespace
bool bind(IDXGISwapChain* chain, IUnknown* device) noexcept {
    if (!chain || !device)
        return false;
    ComPtr<ID3D12CommandQueue> candidate;
    return SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&candidate))) &&
           compatible(chain, candidate.Get()) &&
           SUCCEEDED(chain->SetPrivateDataInterface(ownerKey, candidate.Get()));
}
void clear(IDXGISwapChain* chain) noexcept {
    if (chain)
        chain->SetPrivateDataInterface(ownerKey, nullptr);
}
ComPtr<ID3D12CommandQueue> queue(IDXGISwapChain* chain) noexcept {
    ComPtr<ID3D12CommandQueue> result;
    if (!chain)
        return result;
    ComPtr<IUnknown> stored;
    UINT size = sizeof(IUnknown*);
    if (FAILED(chain->GetPrivateData(ownerKey, &size, stored.GetAddressOf())) || size != sizeof(IUnknown*) ||
        !stored || FAILED(stored.As(&result)) || !compatible(chain, result.Get()))
        result.Reset();
    return result;
}
} // namespace modmenu::overlay::swapchain
