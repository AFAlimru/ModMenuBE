#include "core/I18n.h"
#include "core/Log.h"
#include "core/ModRegistry.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include "core/Util.h"
#include "overlay/CommandHook.h"
#include "overlay/GameInputGuard.h"
#include "overlay/Overlay.h"
#include "ui/SelfConfig.h"
#include "compat/NativeBridge.h"

#include <Windows.h>
#include <exception>

namespace {

using namespace modmenu;

DWORD WINAPI nativeEntryThread(LPVOID) {
    try {
        if (!compat::installNativeBridge())
            log::warn("Optional native entry unavailable; overlay and hotkey remain active");
    } catch (std::exception const& error) {
        log::warn("Optional native entry failed: {}", error.what());
    } catch (...) {
        log::warn("Optional native entry failed unexpectedly");
    }
    return 0;
}

DWORD initThreadImpl() {
    log::init();
    log::info("ModMenuBE initialization worker started");
    // Only public DXGI creation observation is enabled before exact native
    // admission. Scans and image hashing must not make us miss the game chain.
    if (!overlay::observeSwapChainCreation()) {
        log::warn("Early DXGI creation observation unavailable; continuing with Present installation");
    }
    settings().load();
    reloadLanguage();
    ui::registerSelfConfig();
    ModRegistry::instance().rescan();
    overlay::installGameInputGuard();
    overlay::watchLeviLaminaCommands();
    log::info("ModMenuBE loaded from {} ({} mods found)", pathUtf8(paths::selfDir()),
              ModRegistry::instance().mods()->size());

    // Definition hooks must observe the first resource load. Starting this
    // independent worker after our potentially slow hidden graphics probe
    // loses that load on faster game/UI startup paths. Native admission still
    // cannot delay or prevent overlay initialization.
    if (HANDLE worker = CreateThread(nullptr, 0, nativeEntryThread, nullptr, 0, nullptr))
        CloseHandle(worker);
    else
        log::warn("Optional native entry worker unavailable; overlay remains active");

    // Our own hidden DXGI probe can install before the game creates its window
    // and swapchain. Waiting for that window could miss the creation queue.
    for (int attempt = 0; attempt < 5; ++attempt) {
        log::info("Installing DXGI overlay (attempt {})", attempt + 1);
        if (overlay::install()) {
            return 0;
        }
        Sleep(1000);
    }
    overlay::uninstall();
    return 0;
}

DWORD WINAPI initThread(LPVOID) {
    try {
        return initThreadImpl();
    } catch (std::exception const& e) {
        log::error("Initialization refused: {}", e.what());
    } catch (...) {
        log::error("Initialization refused: unexpected failure");
    }
    overlay::uninstall();
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
