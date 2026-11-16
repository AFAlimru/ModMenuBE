#include "ModMenu/Settings.h"
#include "ModMenu/TextEditor.h"
#include "json.hpp"
#include <iostream>
#include <filesystem>
#include <fstream>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
int main(){
 int failures=0;auto check=[&](bool ok,char const* text){if(!ok){++failures;std::cerr<<"FAIL "<<text<<'\n';}};
 nlohmann::json j={{"schemaVersion",3},{"plugin",{{"id","ExamplePlugin"},{"entry","Example.dll"},{"versions",{"1.0"}}}},
 {"config",{{"file","config.json"},{"reload","hot"}}},{"fields",{{{"path","/name"},{"type","text"},{"default","世界"},{"minBytes",0},{"maxBytes",64},{"label","Name"}}}}};
 try{auto d=menu::ParseSettingsDescription(j.dump(),"ExamplePlugin","Example.dll","1.0");check(d.fields.size()==1&&std::get<std::string>(d.fields[0].value)=="世界","public v3 accepts UTF8 strings");}
 catch(std::exception const&e){check(false,e.what());}
 for(int v:{1,2}){j["schemaVersion"]=v;bool refused=false;try{menu::ParseSettingsDescription(j.dump(),"ExamplePlugin","Example.dll","1.0");}catch(...){refused=true;}check(refused,"old versions refuse public text");}
 menu::TextEditor e;e.Begin("A中B",8,false);e.Move(-1);e.Erase(true);check(e.value=="AB"&&e.caret==1,"backspace removes one complete Unicode codepoint");
 e.SelectAll();check(!e.Insert("123456789")&&e.value=="AB","oversized paste preserves original selection");
 check(e.Insert("世代")&&e.value=="世代","UTF8 selection replacement");e.Edge(false);e.Move(1,true);check(e.Selection()=="世","shift caret selects codepoints");e.Erase(false);check(e.value=="代","delete selection");
 namespace fs=std::filesystem;auto root=fs::temp_directory_path()/(L"ModMenuTextV3-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));auto dir=root/L"ExamplePlugin";fs::create_directories(dir);
 auto write=[](fs::path const&p,std::string const&s){std::ofstream(p,std::ios::binary)<<s;};auto read=[](fs::path const&p){std::ifstream f(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
 j["schemaVersion"]=3;write(dir/L"manifest.json",R"({"entry":"Example.dll","version":"1.0"})");write(dir/L"modmenu.settings.json",j.dump());write(dir/L"config.json",R"({"name":"old","unknown":[1,2]})");auto draft=menu::LoadSettings(root,"ExamplePlugin");check(draft.valid,"v3 loads real temporary config");if(draft.valid){check(menu::SetText(draft.fields[0],"D:\\my\\腳本.js"),"paths remain literal UTF8 strings");auto saved=menu::ApplySettings(root,draft);auto loaded=nlohmann::json::parse(read(dir/L"config.json"));check(saved.saved&&loaded["name"]=="D:\\my\\腳本.js"&&loaded["unknown"]==nlohmann::json::array({1,2}),"transactional v3 save preserves unknown members");draft=menu::LoadSettings(root,"ExamplePlugin");auto original=read(dir/L"config.json");menu::SetText(draft.fields[0],"cancelled");check(read(dir/L"config.json")==original,"draft edit does not save");write(dir/L"config.json",R"({"name":"external"})");check(!menu::ApplySettings(root,draft).saved,"stale v3 Apply refused");}
 std::cout<<"text contract failures="<<failures<<'\n';return failures?1:0;
}
