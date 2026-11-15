#include "ModMenu/NativeProfile.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <iostream>
#include <vector>
#include <cstring>
int checks{},failures{};void Check(bool b,char const* n){++checks;if(!b){++failures;std::cerr<<"FAIL "<<n<<'\n';}}
int wmain(int argc,wchar_t** argv){using namespace menu;
 if(argc>2){std::cerr<<"Usage: ModMenuNativeTests.exe [private-game-images-root]\n";return 2;}
 Check(AcceptedIdentity({0x8664,0x6AB54E37,0x12C01000,0x128CD801}),"original identity");
 Check(AcceptedIdentity({0x8664,0x6AB54E37,0x12C02000,0x128DA293}),"launcher identity");
 Check(AcceptedIdentity({0x8664,0x6AD79EE4,0x1383B000,0x134D2DEF}),"60 independently verified original identity");
 Check(AcceptedIdentity({0x8664,0x6AD79EE4,0x1383C000,0x134D3CBA}),"60 independently verified launcher identity");
 Check(AcceptedIdentity({0x8664,0x6AD79EE4,0x13847000,0x134E7C20}),"60 exact PreLoader/LeviLauncher image identity");
 Check(AcceptedIdentity({0x8664,0x6AD79EE4,0x1383C000,0x134D3CBB}),"60 console-first regenerated launcher identity");
 Check(!AcceptedIdentity({0x8664,0x6AD79EE4,0x1383C000,0x134D3CBC}),"60 unknown checksum cannot select native authority");
 Check(!AcceptedIdentity({0x8664,0x6AB54E37,0x12C01000,1}),"checksum mismatch refused");
 std::array<std::byte,32> a{},b{};Check(MatchEntry(a,b),"entry equality");b[0]=std::byte{0xe9};Check(!MatchEntry(a,b),"patched entry rejected");Check(!MatchEntry(a,std::span(b).first(8)),"short signature refused");
 ProfileDiagnostic d;Check(!VerifyLoadedEntry(123,a,b,d) && d.rva==123 && d.failure==ProfileFailure::EntryMismatch,"diagnostic records conflict");
 Check(!VerifyFile(a),"nonPE refused");
 for(auto build:{GameBuild::V45,GameBuild::V51,GameBuild::V52}){
  auto layout=LayoutFor(build);Check(layout!=nullptr,"supported build owns a native layout");
  auto pointer=ResolveNativeRva(build,0x48560C0);
  Check(pointer==(build==GameBuild::V45?0x4B1C450u:build==GameBuild::V51?0x4855FF0u:0x48560C0u),"pointer hook resolves independently per build");
  Check(ResolveNativeRva(build,0x416FC0)==(build==GameBuild::V45?0x43D0800u:0x416FC0u),"definition caller resolves independently");
  Check(ResolveNativeRva(build,0x48587D0)==(build==GameBuild::V45?0x4B1D100u:build==GameBuild::V51?0x4858700u:0x48587D0u),"45 wheel hooks outer scene thunk not view implementation");
  Check(ResolveNativeRva(build,0x485A220)==(build==GameBuild::V45?0x4B1D200u:build==GameBuild::V51?0x485A150u:0x485A220u),"45 text hooks outer scene thunk");
  Check(ResolveNativeRva(build,0x123456)==0,"unverified address is never passed through");
  if(layout){Check(layout->viewNameOwner==(build==GameBuild::V45?72u:80u),"name ownership uses correct version layout");
   Check(layout->clientStack==(build==GameBuild::V45?1480u:1488u) && layout->stackTop==(build==GameBuild::V45?376u:424u),"per-client scene-stack branch layout");
   Check(layout->gameTop==(build==GameBuild::V45?384u:424u),"45 fallback stack uses different top virtual slot");
   Check(layout->guiGetter==(build==GameBuild::V45?1736u:1744u) && layout->appOrigin==(build==GameBuild::V45?1544u:1552u),"GUI coordinate getters resolve by build");}
 }
 Check(!LayoutFor(GameBuild::Unsupported) && !ResolveNativeRva(GameBuild::Unsupported,0x48560C0),"unsupported image has no address/layout authority");
 {
  auto l=LayoutFor(GameBuild::V60);
  Check(l && l->clientStack==1512 && l->clientGame==456 && l->stackTop==416 && l->gameTop==424,"60 independently reviewed scene stacks and distinct top slots");
  Check(l && l->guiGetter==1728 && l->appOrigin==1536 && l->transitionA==1152 && l->transitionB==1157,"60 coordinate and transition layouts");
  Check(l && l->screenValid==144 && l->screenGui==160 && l->screenClip==112 && l->uiMode==117,"60 screen fields must not reuse old offsets");
  Check(ResolveNativeRva(GameBuild::V60,0x662FED0)==0x6C0A9A0 && ResolveNativeRva(GameBuild::V60,0xE9958B0)==0xF2D18C0,"60 string overload and constructor-established context table");
  Check(ResolveNativeRva(GameBuild::V60,0x48560C0)==0x4CCE060 && ResolveNativeRva(GameBuild::V60,0x416FC0)==0xFD729,"60 exact pointer entry and parser return");
  Check(ResolveNativeRva(GameBuild::V60,0xE9731B0)==0xF2B9330 && !ResolveNativeRva(GameBuild::V60,0x5D9430F),"60 complete client table and omitted passive-only counter");
  Check(NativeGuards(GameBuild::V60).size()==NativeGuards(GameBuild::V52).size(),"60 retains the complete entry-guard set");
 }
 NativeProfile invalid;ProfileDiagnostic rejected;
 Check(!invalid.Initialize(GetModuleHandleW(nullptr),&rejected) && !invalid.Base() && invalid.Build()==GameBuild::Unsupported && !invalid.Address(0x48560C0),"test host cannot grant native addresses after rejected initialization");
 if(argc==1)std::cout<<"Private game-image checks skipped; pass a local image root to include them.\n";
 if(argc==2)for(auto version:{L"1.26.45.01",L"1.26.51.01",L"1.26.52.03",L"1.26.60.05"})for(auto name:{L"Minecraft.Windows.exe.orig",L"Minecraft.Windows.exe"}){
  std::wcout<<L"Checking "<<version<<L"/"<<name<<std::endl;
  auto path=std::wstring(argv[1])+L"\\"+version+L"\\"+name;
  auto file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);LARGE_INTEGER n{};
  if(file==INVALID_HANDLE_VALUE){Check(false,"requested private image is readable");continue;}
  if(!GetFileSizeEx(file,&n) || n.QuadPart<8192){Check(false,"requested private image has a valid size");CloseHandle(file);continue;}
  // Copy-on-write pages mutate only this test's view, never a game file.
  auto map=CreateFileMappingW(file,nullptr,PAGE_WRITECOPY,0,0,nullptr);auto data=static_cast<std::byte*>(MapViewOfFile(map,FILE_MAP_COPY,0,0,0));
  Check(data && VerifyFile({data,std::size_t(n.QuadPart)}),"exact image hash/PE/code accepted");
  if(data){std::vector<std::byte> h(data,data+8192);Check(VerifyLoadedHeaders({data,std::size_t(n.QuadPart)},h,0x180000000),"matching loaded headers");
   auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(h.data());auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(h.data()+dos->e_lfanew);nt->OptionalHeader.ImageBase=0x180000000;
   Check(VerifyLoadedHeaders({data,std::size_t(n.QuadPart)},h,0x180000000),"ASLR normalized");nt->FileHeader.TimeDateStamp^=1;
   Check(!VerifyLoadedHeaders({data,std::size_t(n.QuadPart)},h,0x180000000),"changed loaded headers rejected");
   auto tail=std::size_t(n.QuadPart)-1;data[tail]^=std::byte{1};
   Check(!VerifyFile({data,std::size_t(n.QuadPart)}),"whole-file identity rejects non-code mutation");data[tail]^=std::byte{1};
   auto sourceDos=reinterpret_cast<IMAGE_DOS_HEADER*>(data);auto sourceNt=reinterpret_cast<IMAGE_NT_HEADERS64*>(data+sourceDos->e_lfanew);
   if(std::wstring(version)==L"1.26.51.01"){
    // LeviLauncher changes only this PE field when its console option changes.
    // Both expectations exercise the real full-file guard on private pages.
    auto subsystem=sourceNt->OptionalHeader.Subsystem;
    sourceNt->OptionalHeader.Subsystem=IMAGE_SUBSYSTEM_WINDOWS_GUI;
    Check(VerifyFile({data,std::size_t(n.QuadPart)}),"51 exact GUI image remains admitted after a console launch");
    sourceNt->OptionalHeader.Subsystem=IMAGE_SUBSYSTEM_WINDOWS_CUI;
    bool preloaderOriginal=sourceNt->OptionalHeader.SizeOfImage==0x12C0C000;
    Check(VerifyFile({data,std::size_t(n.QuadPart)})==!preloaderOriginal,"51 reviewed console forms admitted; old unreviewed console original refused");
    sourceNt->OptionalHeader.Subsystem=IMAGE_SUBSYSTEM_NATIVE;
    Check(!VerifyFile({data,std::size_t(n.QuadPart)}),"51 arbitrary subsystem is not normalized or accepted");
    sourceNt->OptionalHeader.Subsystem=subsystem;
   }
   if(std::wstring(version)==L"1.26.60.05"){
    auto subsystem=sourceNt->OptionalHeader.Subsystem;auto checksum=sourceNt->OptionalHeader.CheckSum;
    // Console enabled before/after .orig capture produces different launcher
    // checksums. Exercise each reviewed full image in private pages only.
    bool launcher=sourceNt->OptionalHeader.SizeOfImage==0x1383C000;
    for(auto sum:launcher?std::vector<DWORD>{0x134D3CBA,0x134D3CBB}:std::vector<DWORD>{checksum}){
     sourceNt->OptionalHeader.CheckSum=sum;
     for(WORD sub:std::array<WORD,2>{IMAGE_SUBSYSTEM_WINDOWS_GUI,IMAGE_SUBSYSTEM_WINDOWS_CUI}){
      sourceNt->OptionalHeader.Subsystem=sub;
      Check(VerifyFile({data,std::size_t(n.QuadPart)}),"60 exact console capture/rebuild order admitted");
     }
    }
    sourceNt->OptionalHeader.Subsystem=IMAGE_SUBSYSTEM_NATIVE;
    Check(!VerifyFile({data,std::size_t(n.QuadPart)}),"60 arbitrary subsystem refused");
    sourceNt->OptionalHeader.Subsystem=subsystem;sourceNt->OptionalHeader.CheckSum=checksum;
    // Full-file checks protect non-code content too, including the launcher.
    auto sections=IMAGE_FIRST_SECTION(sourceNt);
    for(unsigned i=0;i<sourceNt->FileHeader.NumberOfSections;++i){
     if(!sections[i].SizeOfRawData || !std::memcmp(sections[i].Name,".text\0\0\0",8))continue;
     auto at=sections[i].PointerToRawData;data[at]^=std::byte{1};
     Check(!VerifyFile({data,std::size_t(n.QuadPart)}),"60 modified data/import/launcher section refused");
     data[at]^=std::byte{1};
    }
   }
   auto section=IMAGE_FIRST_SECTION(sourceNt);std::size_t code{};
   for(unsigned i=0;i<sourceNt->FileHeader.NumberOfSections;++i)if(!std::memcmp(section[i].Name,".text\0\0\0",8))code=section[i].PointerToRawData;
   if(code){data[code]^=std::byte{1};Check(!VerifyFile({data,std::size_t(n.QuadPart)}),"text identity rejects mutated code");data[code]^=std::byte{1};}
   else Check(false,"exact source contains text section");
   auto checksum=sourceNt->OptionalHeader.CheckSum;sourceNt->OptionalHeader.CheckSum=1;
   Check(!VerifyFile({data,std::size_t(n.QuadPart)}),"unsupported PE identity cannot select another profile");sourceNt->OptionalHeader.CheckSum=checksum;
   Check(VerifyFile({data,std::size_t(n.QuadPart)}),"restored private view still admits exact original image");UnmapViewOfFile(data);
  }if(map)CloseHandle(map);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
 }
 std::cout<<checks<<" checks / "<<failures<<" failures\n";return failures?1:0;
}
