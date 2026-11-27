// Standalone dependency test. Never loads ModMenuBE or starts Minecraft.
#include "overlay/D3D12Runtime.h"

#include <cstdio>
#include <string>

int wmain(int argc, wchar_t** argv) {
    HMODULE shadow{};
    if (argc > 1) {
        // Inspect the player's app-local DLL without executing its DllMain or
        // exports. Keeping it mapped also catches accidental basename reuse.
        shadow = LoadLibraryExW(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
        if (!shadow) {
            std::puts("FAIL: could not map app-local dependency fixture");
            return 1;
        }
        if (!GetProcAddress(shadow, "D3D12CreateDevice") || GetProcAddress(shadow, MAKEINTRESOURCEA(101))) {
            std::puts("FAIL: fixture does not reproduce the ordinal mismatch");
            return 1;
        }
    }
    const auto create = modmenu::overlay::systemD3D12CreateDevice();
    if (!create) {
        std::printf("FAIL: system runtime resolution (%lu)\n", GetLastError());
        return 1;
    }
    HMODULE owner{};
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(create), &owner))
        return 1;
    wchar_t actual[MAX_PATH + 32]{}, system[MAX_PATH + 32]{};
    if (!GetModuleFileNameW(owner, actual, MAX_PATH + 32) || !GetSystemDirectoryW(system, MAX_PATH))
        return 1;
    const std::wstring expected = std::wstring(system) + L"\\d3d12.dll";
    if (_wcsicmp(actual, expected.c_str()) != 0 || owner == shadow ||
        reinterpret_cast<FARPROC>(create) != GetProcAddress(owner, "D3D12CreateDevice")) {
        std::fwprintf(stderr, L"FAIL: resolved wrong dependency: %ls\n", actual);
        return 1;
    }
    if (create != modmenu::overlay::systemD3D12CreateDevice())
        return 1;
    if (shadow)
        FreeLibrary(shadow);
    std::wprintf(L"PASS: D3D12CreateDevice resolved by name from %ls (shadow mapped: %ls)\n", actual,
                 argc > 1 ? L"yes" : L"no");
    return 0;
}
