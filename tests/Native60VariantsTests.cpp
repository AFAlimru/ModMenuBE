#include "ModMenu/NativeProfile.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {
int checks{}, failures{};
void Check(bool ok, char const* message) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "FAIL " << message << '\n';
    }
}

void Image(std::filesystem::path const& path) {
    using namespace menu;
    std::wcout << L"Checking " << path.filename().c_str() << std::endl;
    HANDLE file =
        CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        Check(false, "requested LL60 image readable");
        return;
    }
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file, &length) || length.QuadPart < 8192) {
        CloseHandle(file);
        Check(false, "LL60 image size");
        return;
    }
    HANDLE map = CreateFileMappingW(file, nullptr, PAGE_WRITECOPY, 0, 0, nullptr);
    auto data = map ? static_cast<std::byte*>(MapViewOfFile(map, FILE_MAP_COPY, 0, 0, 0)) : nullptr;
    CloseHandle(file);
    if (!data) {
        if (map)
            CloseHandle(map);
        Check(false, "LL60 image mapping");
        return;
    }
    // All negative cases modify private pages. Installed files are read-only.
    std::span<std::byte const> bytes(data, static_cast<std::size_t>(length.QuadPart));
    Check(VerifyFile(bytes), "observed complete LL60 image admitted");
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(data);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(data + dos->e_lfanew);
    auto subsystem = nt->OptionalHeader.Subsystem;
    for (WORD sub : {WORD(IMAGE_SUBSYSTEM_WINDOWS_GUI), WORD(IMAGE_SUBSYSTEM_WINDOWS_CUI)}) {
        nt->OptionalHeader.Subsystem = sub;
        Check(VerifyFile(bytes), "reviewed exact GUI/console complete hash admitted");
    }
    nt->OptionalHeader.Subsystem = IMAGE_SUBSYSTEM_NATIVE;
    Check(!VerifyFile(bytes), "arbitrary LL60 subsystem refused");
    nt->OptionalHeader.Subsystem = subsystem;
    auto checksum = nt->OptionalHeader.CheckSum;
    nt->OptionalHeader.CheckSum ^= 0x100000;
    Check(!VerifyFile(bytes), "unknown LL60 checksum refused");
    nt->OptionalHeader.CheckSum = checksum;
    auto sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (!sections[i].SizeOfRawData)
            continue;
        auto at = sections[i].PointerToRawData;
        data[at] ^= std::byte{1};
        Check(!VerifyFile(bytes), "changed original/import/launcher section refused");
        data[at] ^= std::byte{1};
    }
    auto tail = bytes.size() - 1;
    data[tail] ^= std::byte{1};
    Check(!VerifyFile(bytes), "changed complete-image tail refused");
    data[tail] ^= std::byte{1};

    for (auto const& guard : NativeGuards(GameBuild::V60)) {
        bool found = false;
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            auto const& s = sections[i];
            if (guard.rva < s.VirtualAddress || guard.rva - s.VirtualAddress + 32 > s.SizeOfRawData)
                continue;
            auto entry = bytes.subspan(s.PointerToRawData + guard.rva - s.VirtualAddress, 32);
            Check(MatchEntry(entry, std::as_bytes(std::span(guard.bytes))),
                  "retained exact IDA entry matches LL60 file");
            ProfileDiagnostic diagnostic;
            auto changed = guard.bytes;
            changed[0] ^= 1;
            Check(!VerifyLoadedEntry(guard.rva, std::as_bytes(std::span(changed)), entry, diagnostic) &&
                      diagnostic.failure == ProfileFailure::EntryMismatch && diagnostic.rva == guard.rva,
                  "live entry conflict still refused at every retained guard");
            found = true;
            break;
        }
        Check(found, "retained guard remains mapped at the same RVA");
    }
    std::vector<std::byte> headers(data, data + 8192);
    Check(VerifyLoadedHeaders(bytes, headers, 0x180000000), "LL60 matching loaded headers admitted");
    auto liveNt = reinterpret_cast<IMAGE_NT_HEADERS64*>(headers.data() + dos->e_lfanew);
    liveNt->OptionalHeader.ImageBase = 0x180000000;
    Check(VerifyLoadedHeaders(bytes, headers, 0x180000000), "only ASLR base normalized");
    liveNt->OptionalHeader.CheckSum ^= 1;
    Check(!VerifyLoadedHeaders(bytes, headers, 0x180000000), "LL60 mismatched loaded header refused");
    Check(VerifyFile(bytes), "restored complete LL60 image remains admitted");
    UnmapViewOfFile(data);
    CloseHandle(map);
}
} // namespace

int wmain(int argc, wchar_t** argv) {
    using namespace menu;
    bool singleImage = argc == 3 && std::wstring(argv[1]) == L"--image";
    if (argc != 1 && argc != 2 && !singleImage) {
        std::cerr << "Usage: Native60VariantsTests [LL60-instance-directory | --image PE-file]\n";
        return 2;
    }
    Check(AcceptedIdentity({0x8664, 0x6AD79EE4, 0x13846000, 0x134E6565}), "LL60 PreLoader original identity");
    Check(AcceptedIdentity({0x8664, 0x6AD79EE4, 0x13847000, 0x134DC051}),
          "LL60 PreLoader plus launcher identity");
    Check(!AcceptedIdentity({0x8664, 0x6AD79EE4, 0x13847000, 0x134DC052}), "unknown LL60 identity refused");
    Check(AcceptedIdentity({0x8664, 0x6AD79EE4, 0x13847000, 0x134E7C20}),
          "previous LL60 exact identity retained");
    Check(AcceptedIdentity({0x8664, 0x6AD79EE4, 0x13847000, 0x134E7C21}),
          "reported launcher-before-PreLoader console identity");
    Check(NativeGuards(GameBuild::V60).size() == NativeGuards(GameBuild::V52).size(),
          "complete native guard set retained");
    Check(ResolveNativeRva(GameBuild::V60, 0x416F40) == 0xFD6B0,
          "LL60 retains independently reviewed definition RVA");
    if (argc == 2) {
        auto directory = std::filesystem::path(argv[1]);
        Image(directory / L"Minecraft.Windows.exe.orig");
        Image(directory / L"Minecraft.Windows.exe");
    }
    if (singleImage)
        Image(std::filesystem::path(argv[2]));
    std::cout << checks << " checks / " << failures << " failures\n";
    return failures ? 1 : 0;
}
