#include "D3D12Runtime.h"

#include <array>
#include <cstring>

namespace modmenu::overlay {
namespace {

struct Runtime {
    PFN_D3D12_CREATE_DEVICE create{};
    DWORD error{ERROR_SUCCESS};
};

Runtime loadRuntime() noexcept {
    std::array<wchar_t, MAX_PATH + 32> path{};
    const UINT length = GetSystemDirectoryW(path.data(), MAX_PATH);
    if (!length)
        return {nullptr, GetLastError()};
    if (length >= MAX_PATH)
        return {nullptr, ERROR_INSUFFICIENT_BUFFER};
    constexpr wchar_t suffix[] = L"\\d3d12.dll";
    std::memcpy(path.data() + length, suffix, sizeof(suffix));

    // An absolute system path also works when the process has already mapped a
    // different d3d12.dll. A basename lookup alone could reuse that module.
    HMODULE module = LoadLibraryExW(path.data(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module)
        return {nullptr, GetLastError()};
    auto create = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(GetProcAddress(module, "D3D12CreateDevice"));
    if (!create) {
        const DWORD error = GetLastError();
        FreeLibrary(module);
        return {nullptr, error};
    }
    return {create, ERROR_SUCCESS}; // Own one reference until process exit.
}

} // namespace

PFN_D3D12_CREATE_DEVICE systemD3D12CreateDevice() noexcept {
    static const Runtime runtime = loadRuntime();
    SetLastError(runtime.error);
    return runtime.create;
}

} // namespace modmenu::overlay
