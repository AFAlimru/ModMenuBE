// Inert game/companion mappings only. Neither DLL entrypoint nor game code runs.
// The sole executed companion accessor is replaced on private pages by a
// six-byte fixture returning a capability bit; the original is never invoked.
#include <Windows.h>
namespace fixture {
unsigned char* readyCode{};
unsigned waits{};
unsigned publishAfter{1};
ULONGLONG clockValue{};
ULONGLONG WINAPI clock() {
    return clockValue;
}
void (*onWait)(){};
HMODULE game{}, companion{};
bool hookError{};
void WINAPI wait(DWORD duration) {
    ++waits;
    clockValue += duration;
    if (onWait) {
        auto action = onWait;
        onWait = nullptr;
        action();
    }
    if (readyCode && waits >= publishAfter) {
        readyCode[1] = 4;
        FlushInstructionCache(GetCurrentProcess(), readyCode, 6);
    } else if (!readyCode)
        ::Sleep(duration);
}
} // namespace fixture
#define Sleep fixture::wait
#define GetTickCount64 fixture::clock
#include "../src/compat/NativeBridge.cpp"
#undef Sleep
#undef GetTickCount64
#include <iostream>

namespace modmenu::ui {
bool isOpen() { return false; }
bool blocksGameInput() { return false; }
void requestOpen(std::string, bool) {}
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::cout << "Private owner audit skipped; pass exact 26.52 EXE and frozen SodiumExtra DLL.\n";
        return 0;
    }
    using namespace modmenu::compat;
    using namespace menu;
    auto image = LoadLibraryExW(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    auto owner = LoadLibraryExW(argv[2], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    auto accessor = owner ? GetProcAddress(owner, "SE_GetCapabilities") : nullptr;
    if (!image || !owner || !accessor)
        return 2;
    int checks = 0, failures = 0;
    auto check = [&](bool value, char const* message) {
        ++checks;
        if (!value) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    };
    check(HashFile(owner, 549888, "C80F4F91FD0883D4424D72CEED5D984B90249FAB20AE2701302EC5A46D7CB044"),
          "real production owner audit matches the exact retained companion");
    check(!HashFile(owner, 549889, "C80F4F91FD0883D4424D72CEED5D984B90249FAB20AE2701302EC5A46D7CB044"),
          "incorrect companion size is rejected");
    check(!HashFile(owner, 549888, "D80F4F91FD0883D4424D72CEED5D984B90249FAB20AE2701302EC5A46D7CB044"),
          "incorrect companion digest is rejected");
    auto code = reinterpret_cast<unsigned char*>(accessor);
    unsigned char original[6]{};
    std::memcpy(original, code, 6);
    DWORD protection{};
    if (!VirtualProtect(code, 6, PAGE_EXECUTE_WRITECOPY, &protection))
        return 2;
    unsigned char stub[]{0xB8, 0, 0, 0, 0, 0xC3};
    std::memcpy(code, stub, sizeof stub);
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    auto scene = reinterpret_cast<unsigned char*>(image) + 0x48576B0;
    void* unusedTrampoline{};
    if (MH_Initialize() != MH_OK)
        return 2;
    fixture::readyCode = code;
    NativeProfile timedOut;
    ProfileDiagnostic timeoutDiagnostic;
    check(!InitializeBridgeProfile(timedOut, image, timeoutDiagnostic, 0) && !timedOut.Base() &&
              timeoutDiagnostic.failure == ProfileFailure::CompanionPending && fixture::waits == 0,
          "an exact loaded but unready companion times out inactive without sleeping beyond a zero budget");
    // Start with the loaded exact owner, ready=0, and untouched game entries.
    // Its hook installation is interleaved only when the installer defers.
    fixture::onWait = +[] { fixture::readyCode[1] = 4; };
    NativeProfile beforeHooks;
    ProfileDiagnostic earlyDiagnostic;
    check(InitializeBridgeProfile(beforeHooks, image, earlyDiagnostic) && fixture::waits == 1,
          "early native installer waits for an exact loaded companion even before its first hook is written");
    profile = &beforeHooks;
    DWORD earlyProtection{};
    if (!VirtualProtect(scene, 4096, PAGE_EXECUTE_WRITECOPY, &earlyProtection))
        return 2;
    scene[0x80] ^= 1;
    check(!SharedHudCurrent(), "a previously pristine HUD caller cannot change without an owned admission");
    scene[0x80] ^= 1;
    DWORD earlyIgnored{};
    VirtualProtect(scene, 4096, earlyProtection, &earlyIgnored);
    profile = &nativeProfile;
    fixture::onWait = nullptr;
    fixture::waits = 0;
    code[1] = 0;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    if (MH_CreateHook(scene, code, &unusedTrampoline) != MH_OK || MH_EnableHook(scene) != MH_OK)
        return 2;
    ProfileDiagnostic diagnostic;
    NativeProfile notReady;
    check(!notReady.Initialize(image, &diagnostic, AcceptSharedHudEntry),
          "production admission refuses a known owner without ready HUD capability");
    code[1] = 4;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    NativeProfile wrongWrapper;
    check(!wrongWrapper.Initialize(image, &diagnostic, AcceptSharedHudEntry),
          "an owned relay to another function in the correct companion is still refused");
    code[1] = 0;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    NativeProfile wrongPending;
    check(!InitializeBridgeProfile(wrongPending, image, diagnostic) && fixture::waits == 0,
          "a relay to unrelated owned code never requests a readiness retry");
    code[1] = 4;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    MH_DisableHook(scene);
    MH_RemoveHook(scene);
    // Exact retained binary: its four-entry detour table identifies sceneHook
    // at 0x27250, which dispatches to the source-audited forwarding method.
    if (MH_CreateHook(scene, reinterpret_cast<unsigned char*>(owner) + 0x27250, &unusedTrampoline) != MH_OK ||
        MH_EnableHook(scene) != MH_OK)
        return 2;
    NativeProfile accepted;
    check(accepted.Initialize(image, &diagnostic, AcceptSharedHudEntry) && sharedHudEntries[0].owner == owner,
          "production admission accepts exact pinned owner, owned MinHook relay and intact caller body");
    auto imageHookTarget = reinterpret_cast<unsigned char*>(image) + 0x4561360;
    auto labelHookTarget = reinterpret_cast<unsigned char*>(image) + 0x4566FC0;
    if (MH_CreateHook(imageHookTarget, reinterpret_cast<unsigned char*>(owner) + 0x27270,
                      &unusedTrampoline) != MH_OK ||
        MH_EnableHook(imageHookTarget) != MH_OK ||
        MH_CreateHook(labelHookTarget, reinterpret_cast<unsigned char*>(owner) + 0x27290,
                      &unusedTrampoline) != MH_OK ||
        MH_EnableHook(labelHookTarget) != MH_OK)
        return 2;
    auto pop = reinterpret_cast<unsigned char*>(image) + 0x6637B90;
    if (MH_CreateHook(pop, reinterpret_cast<unsigned char*>(owner) + 0x272B0, &unusedTrampoline) != MH_OK ||
        MH_EnableHook(pop) != MH_OK)
        return 2;
    code[1] = 0;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    fixture::readyCode = code;
    NativeProfile pending;
    check(InitializeBridgeProfile(pending, image, diagnostic),
          "verified installed HUD hooks may finish publishing ready before native admission gives up");
    check(fixture::waits == 1, "pending readiness is retried without admitting an unready companion");
    code[1] = 4;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    NativeProfile completeHud;
    check(completeHud.Initialize(image, &diagnostic, AcceptSharedHudEntry),
          "complete installed Sodium HUD hook set admits native entry (scene and pop together)");
    if (failures) {
        std::cerr << DescribeProfileDiagnostic(diagnostic) << '\n';
        return 1;
    }
    profile = &completeHud;
    check(SharedHudCurrent(), "both audited HUD entries remain current");
    NativeProfile strict;
    check(!strict.Initialize(image, &diagnostic),
          "default profile still refuses the complete hooked dependency set without explicit owner audit");
    for (auto const& entry : sharedHudEntries) {
        auto wrapper = reinterpret_cast<unsigned char*>(owner) + entry.wrapperRva;
        DWORD wrapperProtection{};
        if (!VirtualProtect(wrapper, entry.wrapper.size(), PAGE_EXECUTE_WRITECOPY, &wrapperProtection))
            return 2;
        // In particular, the pop wrapper's second return-address branch must
        // be checked too. No mutated companion instruction is ever executed.
        wrapper[entry.wrapper.size() - 1] ^= 1;
        check(!SharedHudCurrent(), "post-admission wrapper mutation invalidates the full HUD dependency set");
        NativeProfile wrapperChanged;
        check(!wrapperChanged.Initialize(image, &diagnostic, AcceptSharedHudEntry),
              "production admission refuses changed live wrapper bytes despite exact on-disk owner hash");
        wrapper[entry.wrapper.size() - 1] ^= 1;
        DWORD restoreWrapper{};
        VirtualProtect(wrapper, entry.wrapper.size(), wrapperProtection, &restoreWrapper);
    }
    DWORD popProtection{};
    if (!VirtualProtect(pop, 4096, PAGE_EXECUTE_WRITECOPY, &popProtection))
        return 2;
    pop[0x40] ^= 1;
    check(!SharedHudCurrent(), "post-admission pop body mutation invalidates native observation");
    NativeProfile popChanged;
    check(!popChanged.Initialize(image, &diagnostic, AcceptSharedHudEntry),
          "pop changes beyond detour cannot pass production admission");
    pop[0x40] ^= 1;
    DWORD ignoredPop{};
    VirtualProtect(pop, 4096, popProtection, &ignoredPop);
    MH_DisableHook(pop);
    MH_RemoveHook(pop);
    if (MH_CreateHook(pop, code, &unusedTrampoline) != MH_OK || MH_EnableHook(pop) != MH_OK)
        return 2;
    NativeProfile popWrongWrapper;
    check(!popWrongWrapper.Initialize(image, &diagnostic, AcceptSharedHudEntry),
          "pop relay to unrelated owned code remains rejected");
    check(!SharedHudCurrent(), "changed pop relay invalidates an accepted full HUD dependency set");
    MH_DisableHook(pop);
    MH_RemoveHook(pop);
    if (MH_CreateHook(pop, reinterpret_cast<unsigned char*>(owner) + 0x272B0, &unusedTrampoline) != MH_OK ||
        MH_EnableHook(pop) != MH_OK)
        return 2;
    check(SharedHudCurrent(), "restored exact pop relay and body match the admitted snapshot");
    std::int32_t relayOffset{};
    std::memcpy(&relayOffset, scene + 1, 4);
    auto relay = scene + 5 + relayOffset;
    if (relay[0] != 0xFF || relay[1] != 0x25)
        return 2;
    std::int32_t pointerOffset{};
    std::memcpy(&pointerOffset, relay + 2, 4);
    auto relayPointer = relay + 6 + pointerOffset;
    void* savedDestination{};
    std::memcpy(&savedDestination, relayPointer, sizeof savedDestination);
    DWORD relayProtection{};
    if (!VirtualProtect(relayPointer, sizeof savedDestination, PAGE_EXECUTE_READWRITE, &relayProtection))
        return 2;
    std::memcpy(relayPointer, &code, sizeof code);
    check(!SharedHudCurrent(), "changed relay destination invalidates an otherwise unchanged caller entry");
    std::memcpy(relayPointer, &savedDestination, sizeof savedDestination);
    DWORD restoreProtection{};
    VirtualProtect(relayPointer, sizeof savedDestination, relayProtection, &restoreProtection);
    DWORD sceneProtection{};
    if (!VirtualProtect(scene, 4096, PAGE_EXECUTE_WRITECOPY, &sceneProtection))
        return 2;
    scene[0x80] ^= 1;
    check(!SharedHudCurrent(), "post-admission body mutation invalidates the caller snapshot");
    NativeProfile changed;
    check(!changed.Initialize(image, &diagnostic, AcceptSharedHudEntry),
          "production admission refuses modified code beyond the detour");
    code[1] = 0;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    auto waitsBeforeBody = fixture::waits;
    NativeProfile pendingBodyChanged;
    check(!InitializeBridgeProfile(pendingBodyChanged, image, diagnostic) &&
              fixture::waits == waitsBeforeBody,
          "a modified body never requests retry even while its exact owner is unready");
    code[1] = 4;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    scene[0x80] ^= 1;
    auto firstByte = scene[0];
    scene[0] ^= 1;
    check(!SharedHudCurrent(), "post-admission entry mutation invalidates the caller snapshot");
    scene[0] = firstByte;
    DWORD ignored{};
    VirtualProtect(scene, 4096, sceneProtection, &ignored);
    MH_DisableHook(scene);
    MH_RemoveHook(scene);
    MH_DisableHook(pop);
    MH_RemoveHook(pop);
    MH_DisableHook(imageHookTarget);
    MH_RemoveHook(imageHookTarget);
    MH_DisableHook(labelHookTarget);
    MH_RemoveHook(labelHookTarget);
    for (auto& entry : sharedHudEntries)
        entry.owner = nullptr;
    fixture::game = image;
    fixture::companion = owner;
    fixture::waits = 0;
    code[1] = 0;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    fixture::onWait = +[] {
        for (auto const& entry : sharedHudEntries) {
            auto target = reinterpret_cast<unsigned char*>(fixture::game) + entry.rva;
            auto detour = reinterpret_cast<unsigned char*>(fixture::companion) + entry.wrapperRva;
            void* trampoline{};
            if (MH_CreateHook(target, detour, &trampoline) != MH_OK || MH_EnableHook(target) != MH_OK)
                fixture::hookError = true;
        }
    };
    NativeProfile lateHooks;
    check(InitializeBridgeProfile(lateHooks, image, diagnostic) && !fixture::hookError && fixture::waits == 1,
          "pristine-to-hooked startup revalidates after the known companion publishes readiness");
    check(sharedHudEntries[0].owner == owner && sharedHudEntries[1].owner == owner,
          "the revalidated profile owns both newly installed HUD entries");
    lateHooks.Reset();
    check(!lateHooks.Base() && lateHooks.Build() == GameBuild::Unsupported,
          "reset releases admission state before another profile attempt");
    for (auto const& entry : sharedHudEntries) {
        auto target = reinterpret_cast<unsigned char*>(image) + entry.rva;
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
    for (auto& entry : sharedHudEntries)
        entry.owner = nullptr;
    fixture::waits = 0;
    fixture::publishAfter = 60;
    code[1] = 0;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    NativeProfile slowCompanion;
    check(InitializeBridgeProfile(slowCompanion, image, diagnostic) && fixture::waits == 60,
          "normal admission survives exact companion readiness later than two seconds");
    slowCompanion.Reset();
    fixture::waits = 0;
    code[1] = 0;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof stub);
    NativeProfile deadline;
    auto deadlineStart = fixture::clockValue;
    check(!InitializeBridgeProfile(deadline, image, diagnostic, 175) && !deadline.Base() &&
              diagnostic.failure == ProfileFailure::CompanionPending &&
              fixture::clockValue - deadlineStart == 175,
          "a never-ready exact owner stays inactive at its bounded deadline without oversleeping");
    MH_Uninitialize();
    std::memcpy(code, original, sizeof original);
    FlushInstructionCache(GetCurrentProcess(), code, sizeof original);
    VirtualProtect(code, 6, protection, &ignored);
    profile = &nativeProfile;
    FreeLibrary(image);
    FreeLibrary(owner); // the audited owner deliberately remains pinned
    std::cout << "Production companion audit: " << checks << " checks / " << failures << " failures\n";
    return failures ? 1 : 0;
}
