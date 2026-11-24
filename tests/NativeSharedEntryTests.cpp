// Read-only loader mapping: no game entrypoint, imports or plugin code execute.
#include "ModMenu/NativeProfile.h"
#include <Windows.h>
#include <cstring>
#include <iostream>

namespace {
bool ownerApproved = true;
bool auditedScene(menu::NativeProfile const& profile, menu::EntryGuard const& guard) noexcept {
    return ownerApproved && profile.Build() == menu::GameBuild::V52 && guard.rva == 0x48576B0 &&
           profile.CodeMatches(0x48576B5, 0x4857A09 - 0x48576B5);
}
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cout << "Private exact-image mapping skipped; pass the 26.52 game EXE.\n";
        return 0;
    }
    auto image = LoadLibraryExW(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!image)
        return 2;
    int checks = 0, failures = 0;
    auto check = [&](bool value, char const* message) {
        ++checks;
        if (!value) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    };
    menu::ProfileDiagnostic diagnostic;
    menu::NativeProfile pristine;
    check(pristine.Initialize(image, &diagnostic), "exact inert loaded game image is admitted");
    check(pristine.CodeMatches(0x48576B0, 0x4857A09 - 0x48576B0),
          "original scene body matches disk, including the publication caller");
    check(!pristine.CodeMatches(0, 16) && !pristine.CodeMatches(0x48576B0, 0) &&
              !pristine.CodeMatches(0x48576B0, SIZE_MAX), "bounded code comparison refuses invalid ranges");
    auto scene = reinterpret_cast<unsigned char*>(image) + 0x48576B0;
    unsigned char saved[5]{};
    std::memcpy(saved, scene, sizeof saved);
    DWORD previous{};
    if (!VirtualProtect(scene, 4096, PAGE_EXECUTE_WRITECOPY, &previous))
        return 2;
    scene[0] = 0xE9;
    std::memset(scene + 1, 0, 4); // Never executed. Audit callback simulates the verified owner.
    menu::NativeProfile unknown;
    check(!unknown.Initialize(image, &diagnostic) && diagnostic.rva == 0x48576B0,
          "default policy still refuses a detoured scene entry");
    menu::NativeProfile shared;
    check(shared.Initialize(image, &diagnostic, auditedScene),
          "explicit audited caller accepts only the entry detour with the complete body intact");
    ownerApproved = false;
    menu::NativeProfile noOwner;
    check(!noOwner.Initialize(image, &diagnostic, auditedScene), "unknown owner cannot use the exception");
    ownerApproved = true;
    scene[0x80] ^= 1;
    menu::NativeProfile changedBody;
    check(!changedBody.Initialize(image, &diagnostic, auditedScene),
          "a mutation past the ordinary 32-byte guard refuses coexistence");
    scene[0x80] ^= 1;
    scene[0x4857852 - 0x48576B0 - 1] ^= 1;
    menu::NativeProfile changedCaller;
    check(!changedCaller.Initialize(image, &diagnostic, auditedScene),
          "changed publication callsite refuses coexistence");
    scene[0x4857852 - 0x48576B0 - 1] ^= 1;
    auto other = reinterpret_cast<unsigned char*>(image) + 0x662FBE0;
    DWORD otherProtection{};
    if (!VirtualProtect(other, 32, PAGE_EXECUTE_WRITECOPY, &otherProtection))
        return 2;
    other[0] ^= 1;
    menu::NativeProfile changedOther;
    check(!changedOther.Initialize(image, &diagnostic, auditedScene) && diagnostic.rva == 0x662FBE0,
          "another modified dependency cannot use the caller exception");
    other[0] ^= 1;
    DWORD unused{};
    VirtualProtect(other, 32, otherProtection, &unused);
    std::memcpy(scene, saved, sizeof saved);
    VirtualProtect(scene, 4096, previous, &unused);
    FreeLibrary(image);
    std::cout << "Inert loaded-image shared entry: " << checks << " checks / " << failures << " failures\n";
    return failures ? 1 : 0;
}
