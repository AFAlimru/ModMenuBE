#include "overlay/CommandHook.h"

#include "core/Log.h"
#include "core/ModCommands.h"

#include <MinHook.h>
#include <Windows.h>

#include <cstdint>
#include <cstring>
#include <string>

namespace modmenu::overlay {

namespace {

// LeviLamina 26.x exports (MSVC mangling; LeviLamina is built against the same MSVC STL, so std::string and
// std::weak_ptr have the layouts this file reads):
//   CommandHandle& CommandRegistrar::getOrCreateCommand(std::string const& name, std::string const& description,
//       CommandPermissionLevel, CommandFlag, std::weak_ptr<mod::Mod> mod)
//   std::string const& mod::Mod::getName() const
constexpr char const* kGetOrCreateCommand =
    "?getOrCreateCommand@CommandRegistrar@command@ll@@QEAAAEAVCommandHandle@23@AEBV?$basic_string@DU?$char_traits@D@"
    "std@@V?$allocator@D@2@@std@@0W4CommandPermissionLevel@@UCommandFlag@@V?$weak_ptr@VMod@mod@ll@@@6@@Z";
constexpr char const* kModGetName =
    "?getName@Mod@mod@ll@@QEBAAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@XZ";

// x64: `this`, two pointers, then the enum and the 2-byte CommandFlag in 8-byte slots, and the weak_ptr (a
// non-trivial class) as a pointer to the caller's copy, which the callee destroys. Passing every argument through
// unchanged keeps all of that intact.
using GetOrCreateFn = void* (*)(void* self, std::string const* name, std::string const* description,
                               std::uintptr_t requirement, std::uintptr_t flag, void* modWeakPtr);
using GetNameFn     = std::string const& (*)(void const* mod);

GetOrCreateFn gOriginal = nullptr;
GetNameFn     gGetName  = nullptr;

void copyOut(std::string const* s, char* out, size_t cap) {
    size_t n = s->size() < cap - 1 ? s->size() : cap - 1;
    std::memcpy(out, s->data(), n);
    out[n] = 0;
}

// Reads the arguments under SEH: a layout surprise in a future LeviLamina must not take the game down.
bool readArgs(std::string const* name, std::string const* description, void* modWeakPtr, char* nameOut,
              char* descOut, char* modOut, size_t cap) {
    __try {
        copyOut(name, nameOut, cap);
        copyOut(description, descOut, cap);
        void* mod = modWeakPtr ? *static_cast<void**>(modWeakPtr) : nullptr; // weak_ptr = {T*, control block}
        if (!mod || !gGetName) return false;
        copyOut(&gGetName(mod), modOut, cap);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void* detour(void* self, std::string const* name, std::string const* description, std::uintptr_t requirement,
             std::uintptr_t flag, void* modWeakPtr) {
    char cmd[256], desc[512], mod[256];
    if (readArgs(name, description, modWeakPtr, cmd, desc, mod, 256)) {
        ModCommands::instance().detected(mod, cmd, desc);
        log::info("command /{} belongs to {}", cmd, mod);
    }
    return gOriginal(self, name, description, requirement, flag, modWeakPtr);
}

DWORD WINAPI watchThread(LPVOID) {
    HMODULE ll = nullptr;
    // LeviLamina is loaded early by PreLoader and enables mods (which register commands) much later.
    for (int i = 0; i < 36000 && !ll; ++i) { // up to 3 minutes
        ll = GetModuleHandleW(L"LeviLamina.dll");
        if (!ll) Sleep(5);
    }
    if (!ll) return 0;
    auto target = reinterpret_cast<void*>(GetProcAddress(ll, kGetOrCreateCommand));
    gGetName    = reinterpret_cast<GetNameFn>(GetProcAddress(ll, kModGetName));
    if (!target || !gGetName) {
        log::warn("LeviLamina has no command registrar export this version of ModMenuBE knows; mods' commands "
                  "are only listed from modmenu.json");
        return 0;
    }
    auto status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) return 0;
    if (MH_CreateHook(target, reinterpret_cast<void*>(&detour), reinterpret_cast<void**>(&gOriginal)) != MH_OK
        || MH_EnableHook(target) != MH_OK) {
        log::warn("could not hook LeviLamina's command registrar");
        return 0;
    }
    log::info("watching LeviLamina command registration");
    return 0;
}

} // namespace

void watchLeviLaminaCommands() {
    if (HANDLE t = CreateThread(nullptr, 0, watchThread, nullptr, 0, nullptr)) CloseHandle(t);
}

} // namespace modmenu::overlay
