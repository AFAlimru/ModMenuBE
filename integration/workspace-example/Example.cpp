#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "modmenu/ModMenuWorkspace.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
namespace {
using J=nlohmann::json;
ModMenuWorkspaceApi const* api{};std::uint64_t session{},externalRequest{};HWND window{};bool returnRequested{};
std::uint64_t revision{},handled{},progress{};bool running{};std::string status="No commands have run.",name="Example",selected="one";
J results=J::array();
char const* declaration=R"({"schemaVersion":1,"plugin":{"id":"ModMenuWorkspaceExample","entry":"ModMenuWorkspaceExample.dll","versions":["0.1.0-test1"]},"externalTargets":["editor_ui","viewer_ui"],"pages":[{"id":"jobs","label":{"en_US":"Tasks","zh_TW":"任務","zh_CN":"任务"},"controls":[{"id":"name","type":"text","label":{"en_US":"Task name","zh_TW":"任務名稱","zh_CN":"任务名称"},"minBytes":0,"maxBytes":128},{"id":"flag","type":"boolean","label":{"en_US":"Example option","zh_TW":"示範選項","zh_CN":"示范选项"}},{"id":"start","type":"action","label":{"en_US":"Start demo task","zh_TW":"啟動示範任務","zh_CN":"启动示范任务"}},{"id":"reset","type":"action","label":{"en_US":"Reset demo result","zh_TW":"重設示範結果","zh_CN":"重置示范结果"},"confirmation":{"en_US":"Reset only the demo counters? No files are touched.","zh_TW":"只重設示範計數？不會操作檔案。","zh_CN":"只重置示范计数？不会操作文件。"}},{"id":"status","type":"status","label":{"en_US":"Worker status","zh_TW":"工作狀態","zh_CN":"工作状态"}},{"id":"progress","type":"progress","label":{"en_US":"Task progress","zh_TW":"任務進度","zh_CN":"任务进度"}},{"id":"targets","type":"list","label":{"en_US":"Example targets","zh_TW":"示範目標","zh_CN":"示范目标"},"selectAction":"choose","emptyText":"No targets"}]},{"id":"map","label":{"en_US":"Preview","zh_TW":"預覽","zh_CN":"预览"},"controls":[{"id":"map_status","type":"status","label":{"en_US":"Preview status","zh_TW":"預覽狀態","zh_CN":"预览状态"}},{"id":"image","type":"image","label":{"en_US":"Bounded pixel preview","zh_TW":"有界像素預覽","zh_CN":"有界像素预览"}}]}]})";
void terminal(J const&command,bool success,std::string message){results.push_back({{"requestId",command.at("requestId")},{"state",success?"succeeded":"failed"},{"message",std::move(message)}});if(results.size()>16)results.erase(results.begin());}
void publish(){J pixels=J::array();for(unsigned y=0;y<24;++y)for(unsigned x=0;x<32;++x)pixels.push_back(((x/4+y/4)%2)?0x3C8527FFu:0xD0D1D4FFu);
 J state={{"protocolVersion",1},{"revision",++revision},{"controls",J::object()},{"results",results}};
 auto& controls=state["controls"];
 controls["name"]={{"value",name}};controls["flag"]={{"value",false}};
 controls["start"]={{"enabled",!running},{"busy",running}};controls["reset"]={{"enabled",true}};
 controls["status"]={{"text",status+" Handled commands: "+std::to_string(handled)}};
 controls["progress"]={{"value",progress},{"total",100}};
 controls["targets"]={{"rows",J::array({J{{"id","one"},{"label","First target"},{"text",selected=="one"?"Selected":"Available"}},J{{"id","two"},{"label","Second target"},{"text",selected=="two"?"Selected":"Available"}}})}};
 controls["map_status"]={{"text","Static 32 x 24 RGBA8 preview; no network or files."}};
 controls["image"]={{"width",32},{"height",24},{"pixels",std::move(pixels)}};
 auto raw=state.dump();api->publish(session,raw.data(),std::uint32_t(raw.size()));}
LRESULT CALLBACK procedure(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
 if(msg==WM_COMMAND&&LOWORD(w)==101){returnRequested=true;DestroyWindow(hwnd);return 0;}
 if(msg==WM_CLOSE){DestroyWindow(hwnd);return 0;}
 if(msg==WM_DESTROY){window=nullptr;return 0;}
 return DefWindowProcW(hwnd,msg,w,l);
}
bool openWindow(std::string const&target){if(window)return false;WNDCLASSW klass{};klass.lpfnWndProc=procedure;klass.hInstance=GetModuleHandleW(L"ModMenuWorkspaceExample.dll");klass.lpszClassName=L"ModMenuWorkspaceExampleWindow";klass.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));klass.hbrBackground=GetSysColorBrush(COLOR_WINDOW);RegisterClassW(&klass);
 window=CreateWindowExW(0,klass.lpszClassName,target=="editor_ui"?L"Workspace example - Editor":L"Workspace example - Viewer",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,620,270,nullptr,nullptr,klass.hInstance,nullptr);if(!window)return false;
 CreateWindowW(L"STATIC",L"This is the plugin's own UI, opened by an explicit workspace button.\nNo network, login or file operation is performed.\n\nReturn restores the workspace; closing this window does not auto-open it.\nYou can reopen Mods explicitly with F10.",WS_CHILD|WS_VISIBLE,20,20,570,120,window,nullptr,klass.hInstance,nullptr);
 CreateWindowW(L"BUTTON",L"Return to Workspace",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,20,150,240,38,window,reinterpret_cast<HMENU>(101),klass.hInstance,nullptr);ShowWindow(window,SW_SHOW);SetForegroundWindow(window);returnRequested=false;return true;
}
bool ownAddress(HMODULE module,void const*address){HMODULE owner{};return address&&GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&owner)&&owner==module;}
DWORD WINAPI worker(void*){
 HMODULE pinned{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&worker),&pinned);
 for(;;){if(auto menu=GetModuleHandleW(L"ModMenuBE.dll")){auto query=reinterpret_cast<ModMenu_GetWorkspaceApiFn>(GetProcAddress(menu,"ModMenu_GetWorkspaceApi"));if(ownAddress(menu,reinterpret_cast<void*>(query))){auto candidate=query(1);if(candidate&&candidate->version==1&&candidate->structSize>=sizeof(ModMenuWorkspaceApi)&&ownAddress(menu,candidate)&&ownAddress(menu,reinterpret_cast<void*>(candidate->isReady))&&ownAddress(menu,reinterpret_cast<void*>(candidate->unregisterProvider))&&ownAddress(menu,reinterpret_cast<void*>(candidate->registerProvider))&&ownAddress(menu,reinterpret_cast<void*>(candidate->poll))&&ownAddress(menu,reinterpret_cast<void*>(candidate->publish))&&ownAddress(menu,reinterpret_cast<void*>(candidate->acknowledgeExternal))&&ownAddress(menu,reinterpret_cast<void*>(candidate->returnToWorkspace))&&candidate->isReady()==1){HMODULE pinMenu{};if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(query),&pinMenu)&&pinMenu==menu){api=candidate;if(api->registerProvider(reinterpret_cast<void*>(&worker),declaration,std::uint32_t(std::char_traits<char>::length(declaration)),&session)==0)break;}}}}Sleep(250);}
 auto published=GetTickCount64(),step=published;publish();
 for(;;){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
  if(returnRequested){returnRequested=false;api->returnToWorkspace(session,externalRequest);externalRequest=0;}
  std::uint32_t required{};auto code=api->poll(session,nullptr,0,&required);if((code==0||code==1)&&required&&required<=32768){std::vector<char> bytes(required);if(api->poll(session,bytes.data(),required,&required)==0){try{auto command=J::parse(bytes.data());if(command.contains("kind")){auto target=command.at("target").get<std::string>();auto request=command.at("requestId").get<std::uint64_t>();bool opened=(target=="editor_ui"||target=="viewer_ui")&&openWindow(target);if(api->acknowledgeExternal(session,request,opened?1:0)==0&&opened)externalRequest=request;else if(opened)DestroyWindow(window);}else{++handled;auto action=command.at("action").get<std::string>();if(command.contains("values")&&command["values"].contains("name"))name=command["values"]["name"].get<std::string>();if(action=="start"){running=true;progress=0;status="Demo task running.";terminal(command,true,"Demo task accepted.");}else if(action=="reset"){running=false;progress=0;status="Demo counters reset.";terminal(command,true,"Reset finished.");}else if(action=="choose"){selected=command.at("rowId").get<std::string>();terminal(command,true,"Target selected: "+selected);}else terminal(command,false,"Unknown demo action.");publish();}}catch(...){status="Malformed command refused.";}}}
  auto now=GetTickCount64();if(running&&now-step>=200){step=now;if(progress<100)progress+=5;if(progress>=100){progress=100;running=false;status="Demo task completed.";}}
  if(now-published>=500){published=now;publish();}Sleep(25);
 }
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){DisableThreadLibraryCalls(instance);auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr);if(thread)CloseHandle(thread);}return TRUE;}
