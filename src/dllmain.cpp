#include "core/I18n.h"
#include "core/Log.h"
#include "core/ModRegistry.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include "core/Util.h"
#include "overlay/GameInputGuard.h"
#include "overlay/Overlay.h"
#include "ui/SelfConfig.h"

#include <Windows.h>

namespace {

using namespace modmenu;

struct WindowSearch {
    DWORD pid;
    HWND  found;
};

BOOL CALLBACK findWindow(HWND hwnd, LPARAM lp) {
    auto* search = reinterpret_cast<WindowSearch*>(lp);
    DWORD pid    = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == search->pid && IsWindowVisible(hwnd) && GetWindow(hwnd, GW_OWNER) == nullptr) {
        search->found = hwnd;
        return FALSE;
    }
    return TRUE;
}

DWORD WINAPI initThread(LPVOID) {
    log::init();
    settings().load();
    reloadLanguage();
    ui::registerSelfConfig();
    ModRegistry::instance().rescan();
    overlay::installGameInputGuard();
    log::info("ModMenuBE loaded from {} ({} mods found)", pathUtf8(paths::selfDir()),
              ModRegistry::instance().mods()->size());

    // Hook once the game has a window, so DXGI and the game's device exist.
    WindowSearch search{GetCurrentProcessId(), nullptr};
    for (int i = 0; i < 1200 && !search.found; ++i) { // up to 2 minutes
        EnumWindows(findWindow, reinterpret_cast<LPARAM>(&search));
        if (!search.found) Sleep(100);
    }
    if (!search.found) {
        log::error("no game window appeared; overlay not installed");
        return 0;
    }
    for (int attempt = 0; attempt < 5; ++attempt) {
        if (overlay::install()) return 0;
        Sleep(1000);
    }
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        paths::setSelfModule(module);
        // Pin ourselves: hooks and the exported API must outlive any FreeLibrary.
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&DllMain), &pinned);
        if (HANDLE t = CreateThread(nullptr, 0, initThread, nullptr, 0, nullptr)) CloseHandle(t);
    }
    return TRUE;
}
