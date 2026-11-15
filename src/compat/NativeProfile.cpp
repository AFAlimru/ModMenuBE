#include "ModMenu/NativeProfile.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>
namespace menu {
namespace {
#include "NativeGuards.inl"
struct NativeAddress {std::uint32_t key,rva;};
struct ExactImage {ImageIdentity identity;std::size_t bytes;char const* sha256;char const* alternateSha256{};};
struct BuildDescriptor {
 GameBuild build;char const* version;ExactImage images[2];std::size_t textBytes;char const* textSha256;
 std::span<NativeAddress const> addresses;std::span<EntryGuard const> guards;NativeLayout layout;
 std::span<ExactImage const> additionalImages{};
};
constexpr NativeLayout layout52{};
constexpr NativeLayout layout45{1480,416,960,376,384,1736,1544,72,56,1112,1117,820,824};
constexpr NativeLayout layout60{1512,456,968,416,424,1728,1536,80,64,1152,1157,828,832,144,160,112,117};
#include "NativeCompatibility.inl"
BuildDescriptor const* Descriptor(GameBuild build){for(auto const& d:builds)if(d.build==build)return &d;return nullptr;}
bool SameIdentity(ImageIdentity const& a,ImageIdentity const& b){return a.machine==b.machine && a.timestamp==b.timestamp && a.imageSize==b.imageSize && a.checksum==b.checksum;}
BuildDescriptor const* Descriptor(ImageIdentity const& id){
 for(auto const& d:builds){
  for(auto const& image:d.images)if(SameIdentity(id,image.identity))return &d;
  for(auto const& image:d.additionalImages)if(SameIdentity(id,image.identity))return &d;
 }return nullptr;
}
bool Hash(std::span<std::byte const> data,char const* expected){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE handle{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    DWORD objectSize{},used{};
    bool ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof objectSize,&used,0)>=0;
    std::vector<std::uint8_t> object(ok?objectSize:0);std::array<std::uint8_t,32> digest{};
    if(ok)ok=BCryptCreateHash(algorithm,&handle,object.data(),objectSize,nullptr,0,0)>=0;
    if(ok)ok=BCryptHashData(handle,reinterpret_cast<PUCHAR>(const_cast<std::byte*>(data.data())),static_cast<ULONG>(data.size()),0)>=0;
    if(ok)ok=BCryptFinishHash(handle,digest.data(),32,0)>=0;
    if(handle)BCryptDestroyHash(handle);BCryptCloseAlgorithmProvider(algorithm,0);
    constexpr char hex[]="0123456789ABCDEF";
    for(unsigned i=0;ok && i<32;++i)ok=hex[digest[i]>>4]==expected[i*2] && hex[digest[i]&15]==expected[i*2+1];
    return ok;
}
IMAGE_NT_HEADERS64 const* Headers(std::span<std::byte const> data,std::uintptr_t loadedBase=0){
    if(data.size()<sizeof(IMAGE_DOS_HEADER))return nullptr;
    auto dos=reinterpret_cast<IMAGE_DOS_HEADER const*>(data.data());
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<64 || dos->e_lfanew>4096 || data.size()<static_cast<std::size_t>(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS64))return nullptr;
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS64 const*>(data.data()+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
       nt->FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64) ||
       (nt->OptionalHeader.ImageBase!=0x140000000 && (!loadedBase || nt->OptionalHeader.ImageBase!=loadedBase)) ||
       !nt->FileHeader.NumberOfSections || nt->FileHeader.NumberOfSections>96)return nullptr;
    auto end=static_cast<std::size_t>(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS64)+nt->FileHeader.NumberOfSections*sizeof(IMAGE_SECTION_HEADER);
    return end<=data.size()?nt:nullptr;
}
bool ReadMemory(void const* from,void* to,std::size_t size) noexcept{
    __try {if(!from)return false;std::memcpy(to,from,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
}
NativeLayout const* LayoutFor(GameBuild build) noexcept{auto d=Descriptor(build);return d?&d->layout:nullptr;}
std::uint32_t ResolveNativeRva(GameBuild build,std::uint32_t key) noexcept{
 auto d=Descriptor(build);if(d)for(auto const& address:d->addresses)if(address.key==key)return address.rva;return 0;
}
char const* NativeProfile::Version() const noexcept{auto d=Descriptor(build_);return d?d->version:"unsupported";}
bool AcceptedIdentity(ImageIdentity const& id) noexcept{
    return Descriptor(id)!=nullptr;
}
bool MatchEntry(std::span<std::byte const> actual,std::span<std::byte const> expected) noexcept{
    return expected.size()>=16 && expected.size()<=32 && actual.size()>=expected.size() && std::equal(expected.begin(),expected.end(),actual.begin());
}
bool VerifyFile(std::span<std::byte const> data) noexcept{
    try{
        auto nt=Headers(data);if(!nt)return false;
        ImageIdentity id{nt->FileHeader.Machine,nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage,nt->OptionalHeader.CheckSum};
        auto d=Descriptor(id);if(!d)return false;ExactImage const* exact=nullptr;
        for(auto const& image:d->images)if(SameIdentity(id,image.identity))exact=&image;
        for(auto const& image:d->additionalImages)if(SameIdentity(id,image.identity))exact=&image;
        if(!exact || data.size()!=exact->bytes)return false;
        auto section=IMAGE_FIRST_SECTION(nt);bool found=false;
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){
            auto const& s=section[i];
            if(s.PointerToRawData>data.size() || s.SizeOfRawData>data.size()-s.PointerToRawData)return false;
            if(std::memcmp(s.Name,".text\0\0\0",8))continue;
            if(found || s.VirtualAddress!=0x1000 || s.SizeOfRawData!=d->textBytes || !(s.Characteristics&IMAGE_SCN_MEM_EXECUTE))return false;
            found=Hash(data.subspan(s.PointerToRawData,s.SizeOfRawData),d->textSha256);
            if(!found)return false;
            for(auto const& guard:d->guards){
                if(guard.rva<0x1000 || guard.rva-0x1000>s.SizeOfRawData-32 || !MatchEntry(data.subspan(s.PointerToRawData+guard.rva-0x1000,32),std::as_bytes(std::span(guard.bytes))))return false;
            }
        }
        // Alternate complete hashes admit only explicitly reviewed GUI/console
        // forms. Never normalize headers or accept arbitrary PE edits.
        return found && (Hash(data,exact->sha256) ||
            (exact->alternateSha256 && Hash(data,exact->alternateSha256)));
    }catch(...){return false;}
}
std::span<EntryGuard const> NativeGuards(GameBuild build){auto d=Descriptor(build);return d?d->guards:std::span<EntryGuard const>{};}
bool VerifyLoadedHeaders(std::span<std::byte const> disk,std::span<std::byte const> loaded,std::uintptr_t moduleBase) noexcept{
    auto nt=Headers(disk),live=Headers(loaded,moduleBase);
    if(!moduleBase || !nt || !live)return false;
    // Windows may rewrite only the loaded ImageBase to the ASLR address.
    // Normalize that value, then compare the complete NT headers and sections.
    auto normalized=*live;normalized.OptionalHeader.ImageBase=nt->OptionalHeader.ImageBase;
    auto diskDos=reinterpret_cast<IMAGE_DOS_HEADER const*>(disk.data());
    auto liveDos=reinterpret_cast<IMAGE_DOS_HEADER const*>(loaded.data());
    return diskDos->e_lfanew==liveDos->e_lfanew &&
        !std::memcmp(nt,&normalized,sizeof normalized) &&
        !std::memcmp(IMAGE_FIRST_SECTION(nt),IMAGE_FIRST_SECTION(live),nt->FileHeader.NumberOfSections*sizeof(IMAGE_SECTION_HEADER));
}
bool VerifyLoadedEntry(std::uint32_t rva,std::span<std::byte const> actual,std::span<std::byte const> expected,ProfileDiagnostic& diagnostic) noexcept{
    if(MatchEntry(actual,expected))return true;
    diagnostic.failure=ProfileFailure::EntryMismatch;diagnostic.rva=rva;
    diagnostic.expected={};diagnostic.observed={};
    std::copy_n(expected.begin(),std::min(expected.size(),diagnostic.expected.size()),diagnostic.expected.begin());
    std::copy_n(actual.begin(),std::min(actual.size(),diagnostic.observed.size()),diagnostic.observed.begin());
    return false;
}
std::string DescribeProfileDiagnostic(ProfileDiagnostic const& d){
    char const* stage="unknown";
    switch(d.failure){
        case ProfileFailure::None:stage="validated";break;
        case ProfileFailure::Module:stage="invalid-module";break;
        case ProfileFailure::Path:stage="module-path";break;
        case ProfileFailure::Open:stage="image-open";break;
        case ProfileFailure::Map:stage="image-mapping";break;
        case ProfileFailure::Disk:stage="disk-identity";break;
        case ProfileFailure::LoadedRead:stage="loaded-header-read";break;
        case ProfileFailure::LoadedHeaders:stage="loaded-header-mismatch";break;
        case ProfileFailure::EntryRead:stage="loaded-entry-read";break;
        case ProfileFailure::EntryMismatch:stage="loaded-entry-mismatch";break;
    }
    std::ostringstream out;out<<stage<<"; win32="<<d.systemError<<std::uppercase<<std::hex
        <<"; moduleBase=0x"<<d.moduleBase<<"; headerBase=0x"<<d.headerBase;
    if(d.rva)out<<"; rva=0x"<<d.rva;
    if(d.failure==ProfileFailure::EntryMismatch){
        out<<std::setfill('0')<<"; expected=";for(auto b:d.expected)out<<std::setw(2)<<std::to_integer<unsigned>(b);
        out<<"; observed=";for(auto b:d.observed)out<<std::setw(2)<<std::to_integer<unsigned>(b);
    }
    return out.str();
}
bool NativeProfile::Initialize(void* module,ProfileDiagnostic* output) noexcept{
    ProfileDiagnostic local;auto& diagnostic=output?*output:local;diagnostic={};
    diagnostic.moduleBase=reinterpret_cast<std::uintptr_t>(module);
    auto fail=[&](ProfileFailure failure,DWORD error=0){diagnostic.failure=failure;diagnostic.systemError=error;return false;};
    if(base_ || !module)return fail(ProfileFailure::Module);
    build_=GameBuild::Unsupported;
    wchar_t path[32768]{};auto pathLength=GetModuleFileNameW(static_cast<HMODULE>(module),path,32768);
    if(!pathLength || pathLength>=32768)return fail(ProfileFailure::Path,GetLastError());
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return fail(ProfileFailure::Open,GetLastError());
    LARGE_INTEGER length{};if(!GetFileSizeEx(file,&length)){auto error=GetLastError();CloseHandle(file);return fail(ProfileFailure::Map,error);}
    HANDLE mapping=CreateFileMappingW(file,nullptr,PAGE_READONLY,0,0,nullptr);auto error=GetLastError();CloseHandle(file);
    if(!mapping)return fail(ProfileFailure::Map,error);
    auto bytes=static_cast<std::byte const*>(MapViewOfFile(mapping,FILE_MAP_READ,0,0,0));error=GetLastError();CloseHandle(mapping);
    if(!bytes)return fail(ProfileFailure::Map,error);
    if(length.QuadPart<0 || !VerifyFile({bytes,static_cast<std::size_t>(length.QuadPart)})){UnmapViewOfFile(bytes);return fail(ProfileFailure::Disk);}
    disk_=bytes;diskSize_=static_cast<std::size_t>(length.QuadPart);base_=reinterpret_cast<std::uintptr_t>(module);
    auto nt=Headers({disk_,diskSize_});auto s=IMAGE_FIRST_SECTION(nt);sectionCount_=nt->FileHeader.NumberOfSections;
    build_=Descriptor({nt->FileHeader.Machine,nt->FileHeader.TimeDateStamp,nt->OptionalHeader.SizeOfImage,nt->OptionalHeader.CheckSum})->build;
    for(unsigned i=0;i<sectionCount_;++i){
        sections_[i]={s[i].VirtualAddress,s[i].PointerToRawData,s[i].SizeOfRawData,s[i].Characteristics};
        if(s[i].VirtualAddress==0x1000){textOffset_=s[i].PointerToRawData;textSize_=s[i].SizeOfRawData;}
    }
    // Independently verify the loaded headers, not merely the file name.
    std::array<std::byte,8192> loaded{};
    bool valid=ReadMemory(module,loaded.data(),loaded.size());
    if(!valid)diagnostic.failure=ProfileFailure::LoadedRead;
    else {
        auto dos=reinterpret_cast<IMAGE_DOS_HEADER const*>(loaded.data());
        if(dos->e_lfanew>=64 && dos->e_lfanew<=4096)
            diagnostic.headerBase=reinterpret_cast<IMAGE_NT_HEADERS64 const*>(loaded.data()+dos->e_lfanew)->OptionalHeader.ImageBase;
        valid=VerifyLoadedHeaders({disk_,diskSize_},loaded,base_);
        if(!valid)diagnostic.failure=ProfileFailure::LoadedHeaders;
    }
    for(auto const& g:NativeGuards(build_)){
        if(!valid)break;
        // Shared MTB entries are checked separately against a pinned Ready owner.
        if(g.rva==ResolveNativeRva(build_,0x48560C0) || g.rva==ResolveNativeRva(build_,0x48560A0) || g.rva==ResolveNativeRva(build_,0x48560B0))continue;
        std::array<std::byte,32> actual{};
        if(!ReadMemory(reinterpret_cast<void const*>(base_+g.rva),actual.data(),actual.size())){
            diagnostic.failure=ProfileFailure::EntryRead;diagnostic.rva=g.rva;valid=false;
        }else valid=VerifyLoadedEntry(g.rva,actual,std::as_bytes(std::span(g.bytes)),diagnostic);
    }
    if(!valid){UnmapViewOfFile(disk_);disk_=nullptr;base_=0;build_=GameBuild::Unsupported;return false;}
    return true;
}
bool NativeProfile::Entry(std::uintptr_t fn) const noexcept{
    if(!base_ || !disk_ || fn<base_+0x1000 || fn-base_-0x1000>textSize_-32)return false;
    std::array<std::byte,32> live{};
    return ReadMemory(reinterpret_cast<void const*>(fn),live.data(),32) && MatchEntry(live,{disk_+textOffset_+(fn-base_-0x1000),32});
}
bool NativeProfile::Table(void const* object,std::uint32_t rva) const noexcept{
    auto address=Address(rva);std::uintptr_t actual{};return base_ && address && ReadMemory(object,&actual,sizeof actual) && actual==address;
}
std::uintptr_t NativeProfile::Virtual(void const* object,unsigned offset) const noexcept{
    if(!base_ || offset>4096 || offset%8)return 0;
    std::uintptr_t table{},fn{};
    if(!ReadMemory(object,&table,sizeof table) || table<base_ || !ReadMemory(reinterpret_cast<void const*>(table+offset),&fn,sizeof fn))return 0;
    auto rva=table-base_+offset;
    for(unsigned i=0;i<sectionCount_;++i){auto const& s=sections_[i];
        if((s.flags&IMAGE_SCN_MEM_WRITE) || (s.flags&IMAGE_SCN_MEM_EXECUTE) || rva<s.rva || rva-s.rva+8>s.size)continue;
        std::uintptr_t expected{};std::memcpy(&expected,disk_+s.raw+(rva-s.rva),8);
        return expected>=0x140000000 && fn==base_+(expected-0x140000000) && Entry(fn)?fn:0;
    }
    return 0;
}
}
