#include "ModMenu/NativeProfile.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cstring>
#include <iostream>
#include <vector>
namespace {
int checks{}, failures{};
void Check(bool ok, char const* message) {
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL " << message << '\n'; }
}
void Image(wchar_t const* path, bool preloaderOriginal) {
    using namespace menu;
    HANDLE file=CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file==INVALID_HANDLE_VALUE) { Check(false,"private 51 fixture readable"); return; }
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file,&length) || length.QuadPart<8192) { CloseHandle(file); Check(false,"private 51 fixture size"); return; }
    HANDLE map=CreateFileMappingW(file,nullptr,PAGE_WRITECOPY,0,0,nullptr);
    auto data=map?static_cast<std::byte*>(MapViewOfFile(map,FILE_MAP_COPY,0,0,0)):nullptr;
    CloseHandle(file);
    if (!data) { if(map)CloseHandle(map); Check(false,"private 51 fixture mapping"); return; }
    // Every mutation below affects private copy-on-write pages only.
    std::span<std::byte const> bytes(data,static_cast<std::size_t>(length.QuadPart));
    Check(VerifyFile(bytes),"exact 51 fixture admitted");
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(data);
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(data+dos->e_lfanew);
    auto subsystem=nt->OptionalHeader.Subsystem;
    nt->OptionalHeader.Subsystem=IMAGE_SUBSYSTEM_WINDOWS_GUI;
    Check(VerifyFile(bytes),"reviewed exact GUI form admitted");
    nt->OptionalHeader.Subsystem=IMAGE_SUBSYSTEM_WINDOWS_CUI;
    Check(VerifyFile(bytes)==!preloaderOriginal,"only reviewed exact console forms admitted");
    nt->OptionalHeader.Subsystem=IMAGE_SUBSYSTEM_NATIVE;
    Check(!VerifyFile(bytes),"arbitrary subsystem refused");
    nt->OptionalHeader.Subsystem=subsystem;
    auto section=IMAGE_FIRST_SECTION(nt);
    for (unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        if (std::memcmp(section[i].Name,".text\0\0\0",8) && std::memcmp(section[i].Name,".rdata\0\0",8)) continue;
        auto offset=section[i].PointerToRawData;
        data[offset]^=std::byte{1};
        Check(!VerifyFile(bytes),"changed original code or data section refused");
        data[offset]^=std::byte{1};
    }
    auto tail=bytes.size()-1; data[tail]^=std::byte{1};
    Check(!VerifyFile(bytes),"overlay change refused by whole image identity");
    data[tail]^=std::byte{1};
    auto checksum=nt->OptionalHeader.CheckSum; nt->OptionalHeader.CheckSum^=1;
    Check(!VerifyFile(bytes),"unknown checksum refused"); nt->OptionalHeader.CheckSum=checksum;
    std::vector<std::byte> headers(data,data+8192);
    Check(VerifyLoadedHeaders(bytes,headers,0x180000000),"same loaded headers admitted");
    auto liveNt=reinterpret_cast<IMAGE_NT_HEADERS64*>(headers.data()+dos->e_lfanew);
    liveNt->OptionalHeader.ImageBase=0x180000000;
    Check(VerifyLoadedHeaders(bytes,headers,0x180000000),"only ASLR image base normalized");
    liveNt->OptionalHeader.Subsystem^=1;
    Check(!VerifyLoadedHeaders(bytes,headers,0x180000000),"loaded subsystem mismatch refused");
    Check(VerifyFile(bytes),"restored exact fixture remains admitted");
    UnmapViewOfFile(data); CloseHandle(map);
}
}
int wmain(int argc,wchar_t** argv) {
    using namespace menu;
    if(argc!=1 && argc!=4) { std::cerr<<"Usage: ModMenuNative51VariantsTests.exe [reviewed-preloader-original current-original current-launcher]\n"; return 2; }
    Check(AcceptedIdentity({0x8664,0x6AA482FD,0x12C01000,0x128D20F2}),"51 without PreLoader original identity");
    Check(AcceptedIdentity({0x8664,0x6AA482FD,0x12C02000,0x128CEB86}),"51 without PreLoader launcher identity");
    Check(!AcceptedIdentity({0x8664,0x6AA482FD,0x12C02000,0x128CEB87}),"unknown 51 launcher checksum rejected");
    Check(AcceptedIdentity({0x8664,0x6AA482FD,0x12C0C000,0x128E3A0F}),"old reviewed PreLoader original retained");
    Check(AcceptedIdentity({0x8664,0x6AA482FD,0x12C0D000,0x128DA158}),"old reviewed PreLoader launcher retained");
    Check(ResolveNativeRva(GameBuild::V51,0x48560C0)==0x4855FF0,"51 uses its own input hook");
    if(argc==4) { Image(argv[1],true); Image(argv[2],false); Image(argv[3],false); }
    std::cout<<checks<<" checks / "<<failures<<" failures\n";
    return failures?1:0;
}
