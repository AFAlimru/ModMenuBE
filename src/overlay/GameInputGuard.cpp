#include "overlay/GameInputGuard.h"

#include "core/Log.h"
#include "ui/Ui.h"

#include <Windows.h>

namespace modmenu::overlay {

namespace {

// PreLoader C API (pl/SymbolProvider.h, pl/Hook.h).
using ResolveSymbolFn = void* (*)(char const*);
using HookFn          = int (*)(void* target, void* detour, void** original, int priority);
constexpr int kPriorityHighest = 0;

// void MouseDevice::feed(char actionButtonId, schar buttonData, short x, short y,
//                        short dx, short dy, bool forceMotionlessPointer)
constexpr char const* kMouseFeedSymbol = "?feed@MouseDevice@@QEAAXDCFFFF_N@Z";

using MouseFeedFn = void (*)(void* self, char, signed char, short, short, short, short, bool);
// pl_hook keeps this pointer up to date as other mods hook the same function.
void* gOriginalMouseFeed = nullptr;

void mouseFeedDetour(void* self, char button, signed char data, short x, short y, short dx, short dy,
                     bool motionless) {
    if (ui::blocksGameInput()) return;
    reinterpret_cast<MouseFeedFn>(gOriginalMouseFeed)(self, button, data, x, y, dx, dy, motionless);
}

} // namespace

bool installGameInputGuard() {
    static bool installed = false;
    if (installed) return true;
    HMODULE pl = GetModuleHandleW(L"PreLoader.dll");
    if (!pl) {
        log::warn("PreLoader not loaded: the game camera is not locked while the menu is open");
        return false;
    }
    auto resolve = reinterpret_cast<ResolveSymbolFn>(GetProcAddress(pl, "pl_resolve_symbol_silent"));
    auto hook    = reinterpret_cast<HookFn>(GetProcAddress(pl, "pl_hook"));
    if (!resolve || !hook) {
        log::warn("PreLoader has no pl_resolve_symbol_silent/pl_hook");
        return false;
    }
    void* target = resolve(kMouseFeedSymbol);
    if (!target) {
        log::warn("MouseDevice::feed not found; camera lock unavailable");
        return false;
    }
    if (int rc = hook(target, reinterpret_cast<void*>(&mouseFeedDetour), &gOriginalMouseFeed, kPriorityHighest)) {
        log::error("pl_hook(MouseDevice::feed) failed: {}", rc);
        return false;
    }
    installed = true;
    log::info("camera lock installed (MouseDevice::feed at {:p})", target);
    return true;
}

} // namespace modmenu::overlay
