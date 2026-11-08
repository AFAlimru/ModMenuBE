#include "core/Log.h"

#include "core/Paths.h"

#include <Windows.h>

#include <chrono>
#include <cstdio>
#include <mutex>

namespace modmenu::log {

namespace {
std::mutex gMutex;
FILE*      gFile = nullptr;
} // namespace

void init() {
    std::lock_guard lock(gMutex);
    if (gFile) return;
    std::error_code ec;
    auto dir = paths::selfDir() / L"logs";
    std::filesystem::create_directories(dir, ec);
    gFile = _wfsopen((dir / L"latest.log").c_str(), L"w", _SH_DENYWR);
}

void write(char const* level, std::string const& message) {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "%02u:%02u:%02u.%03u %s [ModMenuBE] ", t.wHour, t.wMinute, t.wSecond,
                  t.wMilliseconds, level);
    std::string line = prefix + message + "\n";
    std::lock_guard lock(gMutex);
    if (gFile) {
        std::fputs(line.c_str(), gFile);
        std::fflush(gFile);
    }
    OutputDebugStringA(line.c_str());
}

} // namespace modmenu::log
