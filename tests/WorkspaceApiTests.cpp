#include "modmenu/ModMenuWorkspace.h"
#include "workspace/Service.h"
#include <iostream>
#include <cstddef>
#include <Windows.h>
#include <filesystem>
#include <fstream>
namespace {bool nativeReadyForTest=false;}
namespace modmenu::compat {bool nativeReady(){return nativeReadyForTest;}}
extern "C" int workspaceCHeaderSize(void);
int main(){int failures=0;auto check=[&](bool ok,char const*m){if(!ok){++failures;std::cerr<<"FAIL "<<m<<'\n';}};
 check(workspaceCHeaderSize()==sizeof(ModMenuWorkspaceApi),"public header compiles as C with same ABI");
 auto api=ModMenu_GetWorkspaceApi(1);check(api&&api->version==1&&api->structSize==sizeof(ModMenuWorkspaceApi),"version/size table");check(ModMenu_GetWorkspaceApi(2)==nullptr,"unknown version not admitted");check(offsetof(ModMenuWorkspaceApi,registerProvider)>offsetof(ModMenuWorkspaceApi,isReady),"stable prefix layout");
 std::uint64_t token=99;check(api->registerProvider(nullptr,"{}",2,&token)==-1&&token==0,"invalid owner zeros token");
 check(api->publish(1,"{}",2)==-2&&!api->isReady(),"not-ready refuses provider state");std::uint32_t required=99;check(api->poll(1,nullptr,0,&required)==-2&&required==0,"not-ready poll bounded");check(api->poll(1,nullptr,1,&required)==-1,"inconsistent buffer refused");
 nativeReadyForTest=true;check(api->isReady()==1,"readiness delegated to native bridge");check(api->registerProvider(reinterpret_cast<void*>(&main),"{}",2,&token)==-3&&token==0,"game executable is not provider owner");
 // Exercise actual Win32 owner/path/manifest/descriptor checks with a tiny
 // no-worker fixture. The game and the example DLL are never loaded.
 wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);auto parent=std::filesystem::path(executable).parent_path();
 auto id="WorkspaceOwnerFixture"+std::to_string(GetCurrentProcessId());auto directory=parent.parent_path()/id;check(!std::filesystem::exists(directory),"exclusive test fixture path");std::filesystem::create_directory(directory);
 auto fixture=directory/L"WorkspaceOwnerFixture.dll";std::filesystem::copy_file(parent/L"WorkspaceOwnerFixture.dll",fixture);
 auto write=[&](std::filesystem::path const&file,std::string const&s){std::ofstream out(file,std::ios::binary);out<<s;};
 write(directory/L"manifest.json",menu::WorkspaceJson{{"name",id},{"entry","WorkspaceOwnerFixture.dll"},{"version","1"}}.dump());
 auto module=LoadLibraryW(fixture.c_str());auto owner=GetProcAddress(module,"WorkspaceOwnerAddress");
 auto ownerDeclaration=menu::WorkspaceJson{{"schemaVersion",1},{"plugin",{{"id",id},{"entry","WorkspaceOwnerFixture.dll"},{"versions",{"1"}}}},{"pages",menu::WorkspaceJson::array()},{"externalTargets",{"editor"}}}.dump();
 check(api->registerProvider(reinterpret_cast<void*>(owner),ownerDeclaration.data(),std::uint32_t(ownerDeclaration.size()),&token)==-3,"missing opt-in refuses actual DLL");
 write(directory/L"modmenu.json",R"({"workspace":false})");check(api->registerProvider(reinterpret_cast<void*>(owner),ownerDeclaration.data(),std::uint32_t(ownerDeclaration.size()),&token)==-3,"false declaration refuses actual DLL");
 write(directory/L"modmenu.json",R"({"workspace":{"version":1,"entries":[{"id":"editor","type":"external","target":"editor","label":"Editor"}]}})");
 check(api->registerProvider(reinterpret_cast<void*>(owner),ownerDeclaration.data(),std::uint32_t(ownerDeclaration.size()),&token)==0&&token,"real DLL identity and explicit declaration register");
 check(api->unregisterProvider(token)==0,"real DLL unregister");FreeLibrary(module);
 // Registration pins the DLL until process exit; only removable text files
 // are removed here. The isolated fixture directory is recorded for cleanup.
 std::filesystem::remove(directory/L"manifest.json");std::filesystem::remove(directory/L"modmenu.json");
 std::string declaration=R"({"schemaVersion":1,"plugin":{"id":"Example","entry":"Example.dll","versions":["1"]},"pages":[],"externalTargets":["editor"]})";
 auto session=modmenu::workspace::service().registerProvider(declaration,"Example","Example.dll","1");check(session!=0,"real service fixture");
 auto request=modmenu::workspace::service().beginExternal(session,"editor",modmenu::workspace::milliseconds());check(request!=0,"explicit service request");modmenu::workspace::service().releaseExternal(session,request,modmenu::workspace::milliseconds());
 check(api->poll(session,nullptr,0,&required)==1&&required>0,"API reports short buffer without dequeue");std::vector<char> bytes(required);check(api->poll(session,bytes.data(),required,&required)==0,"API copies command");
 check(api->acknowledgeExternal(session,request,2)==-1,"invalid result flag refused");check(api->acknowledgeExternal(session,request,1)==0,"API acknowledges open");check(api->returnToWorkspace(session,request)==0,"explicit return");check(api->returnToWorkspace(session,request)==-3,"duplicate return refused");check(api->unregisterProvider(session)==0,"API unregister");check(api->poll(session,nullptr,0,&required)==-3,"stale API session refused");
 std::cout<<"workspace API failures="<<failures<<'\n';return failures?1:0;}
