#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <d3d12.h>
#include <wrl/client.h>

namespace modmenu::overlay {
// Serialized by the overlay lifecycle mutex. Flush is submission, never proof
// of completion. A timed-out marker and its COM/event owners remain alive.
class GpuCompletion {
  public:
    ~GpuCompletion();
    GpuCompletion() = default;
    GpuCompletion(GpuCompletion const&) = delete;
    GpuCompletion& operator=(GpuCompletion const&) = delete;
    HRESULT initialize(ID3D11Device*, ID3D11DeviceContext*, ID3D12CommandQueue* = nullptr);
    HRESULT wait(UINT timeoutMs = 2000);
    bool reset(); // refuses while a completion marker remains unresolved
  private:
    Microsoft::WRL::ComPtr<ID3D11Device> device11_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<ID3D11Query> query_;
    Microsoft::WRL::ComPtr<ID3D12Device> device12_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE event_{};
    UINT64 value_{};
    bool pending_{};
};
} // namespace modmenu::overlay
