#include "core/Paths.h"

#include <string>

namespace modmenu::paths {

namespace {
HMODULE gModule = nullptr;
}

void setSelfModule(HMODULE module) { gModule = module; }
HMODULE selfModule() { return gModule; }

std::filesystem::path selfDir() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        DWORD n = GetModuleFileNameW(gModule, buffer.data(), DWORD(buffer.size()));
        if (n == 0) return std::filesystem::current_path();
        if (n < buffer.size()) {
            buffer.resize(n);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer).parent_path();
}

std::filesystem::path modsDir() { return selfDir().parent_path(); }

} // namespace modmenu::paths
