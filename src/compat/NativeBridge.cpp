// MIT portions extracted from ModMenuBedrock 10f80f2; no old UI renderer or Workspace.
#include "NativeBridge.h"
#include "ModMenu/NativeProfile.h"
#include "ModMenu/Definition.h"
#include "core/Log.h"
#include "core/Paths.h"
#include <bcrypt.h>
#include <fstream>
#include <vector>
#include <nlohmann/json.hpp>
#include "ui/MenuControl.h"
#include "ui/Ui.h"
#include <Windows.h>
#include <MinHook.h>
#include <intrin.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
namespace modmenu::compat {
namespace {
using namespace menu;
NativeProfile nativeProfile;NativeProfile const*profile=&nativeProfile;
std::atomic<bool> ready{};std::mutex observationMutex;Observation observation;ULONGLONG observedAt{};
std::atomic<int> language{};
enum class MenuScene{Unknown,Main,Pause};
MenuScene ClassifyMenuScene(std::string_view s){return s=="start_screen"?MenuScene::Main:s=="pause_screen"?MenuScene::Pause:MenuScene::Unknown;}
struct NonOwner{void*valid{};void*control{};void*object{};};
bool ReadBytes(void const*p,void*out,std::size_t n)noexcept{__try{if(!p)return false;memcpy(out,p,n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
template<class T>T Read(void const*p,std::size_t offset=0){T out{};if(p)ReadBytes(static_cast<std::byte const*>(p)+offset,&out,sizeof out);return out;}
using Dtor=void*(*)(void*);Dtor originalDtor{};
using Button=void(*)(void*,unsigned,char);Button originalDown{},originalUp{};
using Controller=unsigned(*)(void*,void const*);Controller originalController{};
using DefinitionLoad=void(*)(void*,void const*);DefinitionLoad originalDefinitionLoad{};
using JsonParse=unsigned char(*)(void*,char const*,std::size_t,void*,char);JsonParse originalJsonParse{};
thread_local void*eventScene{};
bool Foreground(){DWORD pid{};auto w=GetForegroundWindow();if(w)GetWindowThreadProcessId(w,&pid);return pid==GetCurrentProcessId();}
bool Valid(NonOwner const& v){return v.object && v.valid && Read<unsigned char>(v.valid)!=0;}
void Release(NonOwner& v){profile->Function<void(*)(void*)>(0x7E380)(&v);v={};}
void* Top(void* stack,bool fallback=false){
    auto valid=Read<void*>(stack,8);if(!valid || !Read<unsigned char>(valid))return nullptr;
    auto const& layout=profile->Layout();auto fn=profile->Virtual(stack,fallback?layout.gameTop:layout.stackTop);return fn?reinterpret_cast<void*(*)(void*)>(fn)(stack):nullptr;
}
// Mirrors the two independently exported 5DC6430 branches. Never call its
// shared AutoClick entry; all virtual destinations must match disk and code.
void* TopFor(void* client){
    auto const& layout=profile->Layout();
    if(auto top=Top(Read<void*>(client,layout.clientStack)))return top;
    auto game=Read<void*>(client,layout.clientGame);auto fn=profile->Virtual(game,layout.gameStack);if(!fn)return nullptr;
    NonOwner stack{};reinterpret_cast<void*(*)(void*,NonOwner*)>(fn)(game,&stack);
    auto top=Valid(stack)?Top(stack.object,true):nullptr;Release(stack);return top;
}
MenuScene ReadMenuName(void* view,NativeLayout const& layout){
    // Exact raw-name ownership/layout from 484F4A0; bounded SSO/heap read.
    auto holder=Read<void*>(view,layout.viewNameOwner),meta=Read<void*>(holder,8);if(!meta)return MenuScene::Unknown;
    auto length=Read<std::size_t>(meta,48),capacity=Read<std::size_t>(meta,56);
    if(length!=12 || capacity<length || capacity>4095)return MenuScene::Unknown;
    auto chars=capacity<16?static_cast<std::byte*>(meta)+32:Read<std::byte*>(meta,32);char name[12]{};
    return ReadBytes(chars,name,12)?ClassifyMenuScene({name,12}):MenuScene::Unknown;
}
std::uint32_t ControllerTable(MenuScene kind){
    return kind==MenuScene::Main?0xE900070:kind==MenuScene::Pause?0xE804440:0;
}
MenuScene NamedMenu(void* scene){
    if(!profile->Table(scene,0xE8E9080))return MenuScene::Unknown;
    auto const& layout=profile->Layout();
    auto view=Read<void*>(scene,64);if(!view || Read<unsigned char>(view,layout.transitionA) || Read<unsigned char>(view,layout.transitionB))return MenuScene::Unknown;
    auto kind=ReadMenuName(view,layout);auto table=ControllerTable(kind);
    return table && profile->Table(Read<void*>(view,layout.viewController),table)?kind:MenuScene::Unknown;
}
bool ReadLanguageCode(void* language,int& out) noexcept{
    // Localization::language code: constructor 45DD40, consumer 45AF30.
    auto length=Read<std::size_t>(language,56),capacity=Read<std::size_t>(language,64);
    if(!language || length!=5 || capacity<length || capacity>4095)return false;
    auto chars=capacity<16?static_cast<char*>(language)+40:Read<char*>(language,40);
    char code[6]{};if(!ReadBytes(chars,code,sizeof code) || code[5])return false;
    for(unsigned i:{0u,1u,3u,4u})if(!((code[i]>='A' && code[i]<='Z') || (code[i]>='a' && code[i]<='z')))return false;
    if(code[2]!='_' && code[2]!='-')return false;
    out=std::string_view(code,5)=="zh_CN"?2:std::string_view(code,5)=="zh_TW"?1:0;return true;
}
bool ReadLocaleLocked(void* singleton,int& out) noexcept{
    // Same outer/inner lock order as 45AF30. Try-locks never block rendering.
    // Holding both read locks keeps holder and payload alive without refcount
    // ownership, native allocation, pointer caching or a new native call.
    auto outer=reinterpret_cast<PSRWLOCK>(static_cast<std::byte*>(singleton)+24);
    if(!TryAcquireSRWLockShared(outer))return false;
    bool ok=false;
    __try{
        auto holder=Read<void*>(singleton,32);
        if(holder){
            auto inner=Read<PSRWLOCK>(holder);
            if(inner && inner!=outer && TryAcquireSRWLockShared(inner)){
                __try{ok=ReadLanguageCode(Read<void*>(holder,8),out);}
                __finally{ReleaseSRWLockShared(inner);}
            }
        }else ok=ReadLanguageCode(Read<void*>(singleton,8),out);
    }__finally{ReleaseSRWLockShared(outer);}
    return ok;
}
bool ReadLocaleSnapshot(void* singleton,int& out) noexcept{
    if(!singleton)return false;
    __try{return ReadLocaleLocked(singleton,out);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
struct DefinitionLifetime {
    std::string bytes;std::string_view target;bool allowed;DefinitionLifetime* previous;
    explicit DefinitionLifetime(std::string_view);
    ~DefinitionLifetime();
};
thread_local DefinitionLifetime* definitionLifetime{};
DefinitionLifetime::DefinitionLifetime(std::string_view name):target(name),allowed(!name.empty()),previous(definitionLifetime){definitionLifetime=this;}
DefinitionLifetime::~DefinitionLifetime(){definitionLifetime=previous;}
unsigned char ParseDefinition(std::uintptr_t caller,void* reader,char const* bytes,std::size_t length,void* json,char comments){
    auto scope=definitionLifetime;char const* input=bytes;auto count=length;
    if(scope && scope->allowed && caller==profile->Address(0x416FC0) && length && length<=1024*1024){
        try{
            std::string source(length,'\0');
            if(ReadBytes(bytes,source.data(),length) &&
               (profile->Build()!=GameBuild::V60 || VerifiedMenu60(source)==scope->target) &&
               InsertModsDefinition(source,scope->bytes,scope->target)){
                input=scope->bytes.c_str();count=scope->bytes.size();
            }
        }catch(...){}
    }
    // The Json reader retains begin/end pointers. scope->bytes is owned by
    // DefinitionLoadHook, and outlives the reader's destruction in 416F40.
    return originalJsonParse(reader,input,count,json,comments);
}
unsigned char JsonParseHook(void* reader,char const* bytes,std::size_t count,void* json,char comments){
    return ParseDefinition(reinterpret_cast<std::uintptr_t>(_ReturnAddress()),reader,bytes,count,json,comments);
}
std::string_view AuditedDefinition60(void const* values){
    // FD6B0 receives 216-byte records. Its callback stores a Json result,
    // not a resource path. Only a single exact audited vanilla payload grants
    // automatic insertion authority; arbitrary names/namespaces never do.
    auto begin=Read<std::uintptr_t>(values),end=Read<std::uintptr_t>(values,8),limit=Read<std::uintptr_t>(values,16);
    if(!begin || end<begin || limit<end || limit-begin>216*256 || (end-begin)%216)return {};

    if(end-begin!=216)return {};
    auto record=reinterpret_cast<void const*>(begin);
    auto length=Read<std::size_t>(record,16),capacity=Read<std::size_t>(record,24);
    if(!length || length>1024*1024 || capacity<length || capacity>1024*1024)return {};
    auto chars=capacity<16?reinterpret_cast<char const*>(record):Read<char const*>(record);
    try{std::string source(length,'\0');return ReadBytes(chars,source.data(),length)?VerifiedMenu60(source):std::string_view{};}
    catch(...){return {};}
}
std::string_view AuditedDefinition(void* callback,void const* values){
    if(!ready.load(std::memory_order_acquire) || !profile->Table(callback,0xE75C408))return {};
    if(profile->Build()==GameBuild::V60)return AuditedDefinition60(values);
    auto name=static_cast<std::byte*>(callback)+8;
    auto length=Read<std::size_t>(name,16),capacity=Read<std::size_t>(name,24);
    // Both audited paths are exactly 20 bytes. Namespace is bound to the
    // path, not inferred from an arbitrary resource's JSON payload.
    std::array<char,20> actual{};
    if(length!=actual.size() || capacity<length || capacity>4095)return {};
    auto chars=capacity<16?reinterpret_cast<char const*>(name):Read<char const*>(name);
    if(!ReadBytes(chars,actual.data(),length))return {};
    auto target=DefinitionNamespace({actual.data(),actual.size()});if(target.empty())return {};
    auto begin=Read<std::uintptr_t>(values),end=Read<std::uintptr_t>(values,8),limit=Read<std::uintptr_t>(values,16);
    // Multiple resource layers may contain modifications/overrides. This
    // initial native transform refuses those rather than reinterpreting them.
    return begin && end>begin && end-begin==48 && limit>=end && limit-begin<=48*256?target:std::string_view{};
}
void DefinitionLoadHook(void* callback,void const* values){
    DefinitionLifetime scope(AuditedDefinition(callback,values));
    originalDefinitionLoad(callback,values);
}

bool HashFile(HMODULE module,std::uint32_t expectedSize,char const* expected){
    wchar_t path[32768]{};auto n=GetModuleFileNameW(module,path,32768);if(!n || n>=32768)return false;
    auto h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);if(h==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length{};bool ok=GetFileSizeEx(h,&length) && length.QuadPart==expectedSize;std::vector<unsigned char> bytes(ok?std::size_t(length.QuadPart):0);DWORD used{};
    if(ok)ok=ReadFile(h,bytes.data(),DWORD(bytes.size()),&used,nullptr) && used==bytes.size();CloseHandle(h);
    if(!ok)return false;BCRYPT_ALG_HANDLE alg{};BCRYPT_HASH_HANDLE hash{};DWORD size{};
    ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(ok)ok=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),4,&used,0)>=0;
    std::vector<unsigned char> object(size);std::array<unsigned char,32> digest{};
    if(ok)ok=BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0)>=0;
    if(ok)ok=BCryptHashData(hash,bytes.data(),ULONG(bytes.size()),0)>=0 && BCryptFinishHash(hash,digest.data(),32,0)>=0;
    if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);
    constexpr char hex[]="0123456789ABCDEF";for(unsigned i=0;ok && i<32;++i)ok=hex[digest[i]>>4]==expected[i*2] && hex[digest[i]&15]==expected[i*2+1];return ok;
}
bool Owned(std::uintptr_t address,HMODULE module){HMODULE owner{};return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&owner) && owner==module;}
bool KnownJump(std::uintptr_t address,HMODULE module){
    std::array<unsigned char,16> bytes{};if(!ReadBytes(reinterpret_cast<void*>(address),bytes.data(),bytes.size()) || bytes[0]!=0xe9)return false;
    std::int32_t delta{};std::memcpy(&delta,bytes.data()+1,4);auto target=address+5+delta;
    if(Owned(target,module))return true;
    if(!ReadBytes(reinterpret_cast<void*>(target),bytes.data(),bytes.size()) || bytes[0]!=0xff || bytes[1]!=0x25)return false;
    std::memcpy(&delta,bytes.data()+2,4);auto actual=Read<std::uintptr_t>(reinterpret_cast<void*>(target+6+delta));return Owned(actual,module);
}
Observation observeScene(void*scene){
 Observation o; o.foreground=Foreground();
 if(!profile->Table(scene,0xE8E9080))return o;
 auto client=Read<void*>(scene,56),validity=Read<void*>(scene,40);
 if(!validity||!Read<unsigned char>(validity)||!profile->Table(client,0xE9731B0))return o;
 auto top=TopFor(client);auto kind=NamedMenu(top);if(kind==MenuScene::Unknown)return o;
 auto topValidity=Read<void*>(top,40);if(!topValidity||!Read<unsigned char>(topValidity)||Read<void*>(top,56)!=client)return o;
 o.eligible=true;o.scene=kind==MenuScene::Main?1:2;o.identity=reinterpret_cast<std::uintptr_t>(top);return o;
}
Observation safeObserve(void*scene)noexcept{__try{return observeScene(scene);}__except(EXCEPTION_EXECUTE_HANDLER){return {};}}
void publish(void*context){if(!ready||!profile->Table(context,0xE9958B0))return;auto o=safeObserve(Read<void*>(context,288));{std::lock_guard lock(observationMutex);observation=o;observedAt=GetTickCount64();}
 if(o.eligible){
  for(auto rva:{0x44E6A0u,0x44E750u,0x45AE40u,0x45AF30u,0x4572D0u,0x45DD40u}){
   if(rva==0x4572D0&&profile->Build()==GameBuild::V45)continue;
   if(!profile->Entry(profile->Address(rva)))return;
  }
  auto singleton=reinterpret_cast<void*>(profile->Address(0x11D2B570));int value=0;if(profile->Table(singleton,0xE75E4A0)&&ReadLocaleSnapshot(singleton,value))language=value;
 }}
void* dtorHook(void*context){if(reinterpret_cast<std::uintptr_t>(_ReturnAddress())==profile->Address(0x4857852)){try{publish(context);}catch(...){ready=false;}}return originalDtor(context);}
void forward(void*scene,unsigned id,char focus,bool down){auto old=eventScene;eventScene=scene;__try{(down?originalDown:originalUp)(scene,id,focus);}__finally{eventScene=old;}}
bool ownsInput(void*s){auto o=ready?safeObserve(s):Observation{};return o.eligible&&o.foreground&&o.identity==reinterpret_cast<std::uintptr_t>(s)&&ui::blocksGameInput();}
void downHook(void*s,unsigned id,char f){if(!ownsInput(s))forward(s,id,f,true);}
void upHook(void*s,unsigned id,char f){if(!ownsInput(s))forward(s,id,f,false);}
unsigned controllerHook(void*controller,void const*event){if(ready&&eventScene&&(profile->Table(controller,0xE900070)||profile->Table(controller,0xE804440))){std::array<unsigned char,18>b{};if(ReadBytes(event,b.data(),b.size())){unsigned tag{},id{};memcpy(&tag,b.data(),4);memcpy(&id,b.data()+8,4);if(tag==1&&IsModsOpenEvent(id)&&b[16]<=2&&b[17]<=2){auto o=safeObserve(eventScene);auto view=Read<void*>(eventScene,64);if(o.eligible&&o.foreground&&o.identity==reinterpret_cast<std::uintptr_t>(eventScene)&&Read<void*>(view,profile->Layout().viewController)==controller){if(b[17]==1&&b[16]!=1&&!ui::isOpen()){ui::requestOpen({},false);return 40;}return 32;}}}}return originalController(controller,event);}
}
bool installNativeBridge(){
 log::info("Local bridge validating exact game image");
 if(GetModuleHandleW(L"ModMenuBedrock.dll")){log::error("Local bridge refused: disable old ModMenuBedrock before testing");return false;}
 bool expectedMouse=false;
 {std::ifstream manifest(paths::modsDir()/L"MouseTweaksBedrock"/L"manifest.json",std::ios::binary);if(manifest){auto j=nlohmann::json::parse(manifest,nullptr,false);if(!j.is_object()){log::error("MouseTweaks manifest invalid; bridge refused");return false;}expectedMouse=j.value("enabled",true);}}
 HMODULE mouse{};int mouseStatus=-1;
 auto started=GetTickCount64();
 for(;;){mouse=GetModuleHandleW(L"MouseTweaksBedrock.dll");
  if(mouse){auto status=reinterpret_cast<int(*)()>(GetProcAddress(mouse,"MTB_GetStatus"));
   bool known=HashFile(mouse,158208,"2F289102D36111AA975671F794D10F2C3C6ACE7B1423466674010FD12C7F4577")||HashFile(mouse,158720,"77425954246FCD765070911FAF6135C7E60A4A5E4946FC84C5A1FDC88C461431");
   if(!known||!status||!Owned(reinterpret_cast<std::uintptr_t>(status),mouse)){log::error("Unreviewed MouseTweaks binary; bridge refused");return false;}
   mouseStatus=status();if(mouseStatus==3){HMODULE pin{};if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(status),&pin)||pin!=mouse)return false;break;}
   if(mouseStatus!=0){log::error("MouseTweaks not Ready; bridge refused");return false;}
  }else if(!expectedMouse)break;
  if(GetTickCount64()-started>120000){log::error("MouseTweaks readiness timeout; bridge refused");return false;}Sleep(100);
 }
 ProfileDiagnostic diagnostic;if(!nativeProfile.Initialize(GetModuleHandleW(nullptr),&diagnostic)){log::error("Local bridge inactive: {}",DescribeProfileDiagnostic(diagnostic));return false;}
 std::array<unsigned,6>rvas{0x416F40,0xE033870,0x662FBE0,0x48560A0,0x48560B0,0x4819AF0};
 for(auto r:rvas)if(!profile->Entry(profile->Address(r))&&!(mouse&&mouseStatus==3&&(r==0x48560A0||r==0x48560B0)&&KnownJump(profile->Address(r),mouse))){log::error("Local bridge refused changed hook at {:x}",r);return false;}
 auto status=MH_Initialize();if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED)return false;
 std::array<void*,6>hooks{reinterpret_cast<void*>(DefinitionLoadHook),reinterpret_cast<void*>(JsonParseHook),reinterpret_cast<void*>(dtorHook),reinterpret_cast<void*>(downHook),reinterpret_cast<void*>(upHook),reinterpret_cast<void*>(controllerHook)};
 std::array<void**,6>originals{reinterpret_cast<void**>(&originalDefinitionLoad),reinterpret_cast<void**>(&originalJsonParse),reinterpret_cast<void**>(&originalDtor),reinterpret_cast<void**>(&originalDown),reinterpret_cast<void**>(&originalUp),reinterpret_cast<void**>(&originalController)};
 std::size_t created{};for(;created<rvas.size();++created){auto target=reinterpret_cast<void*>(profile->Address(rvas[created]));if(MH_CreateHook(target,hooks[created],originals[created])!=MH_OK)break;}
 if(created!=rvas.size()){for(std::size_t i=0;i<created;++i)MH_RemoveHook(reinterpret_cast<void*>(profile->Address(rvas[i])));return false;}
 // Publish before enabling; partially enabled callbacks still delegate correctly.
 ready=true;
 for(auto r:rvas)if(MH_EnableHook(reinterpret_cast<void*>(profile->Address(r)))!=MH_OK){ready=false;for(auto installed:rvas)MH_DisableHook(reinterpret_cast<void*>(profile->Address(installed)));log::error("Local native bridge failed to enable; callbacks inactive");return false;}
 log::info("Local native bridge ready: {}; no Workspace; one ImGui renderer",profile->Version());return true;
}
bool nativeReady(){return ready;}
Observation nativeObservation(){std::lock_guard lock(observationMutex);auto value=observation;bool foreground=Foreground();static bool previousForeground=true;static ULONGLONG returned{};auto now=GetTickCount64();if(foreground&&!previousForeground)returned=now;previousForeground=foreground;value.foreground=foreground;if(foreground&&now-observedAt>500){if(now-returned<1500)value.foreground=false;else value.eligible=false;}return value;}
std::string gameLanguage(){return language==2?"zh_CN":language==1?"zh_TW":"en_US";}
}
