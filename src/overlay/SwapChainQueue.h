#pragma once
#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>
namespace modmenu::overlay::swapchain {
bool bind(IDXGISwapChain*, IUnknown*) noexcept;
void clear(IDXGISwapChain*) noexcept;
Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue(IDXGISwapChain*) noexcept;
} // namespace modmenu::overlay::swapchain
