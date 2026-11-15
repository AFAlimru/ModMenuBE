#include "compat/PassiveSettings.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
int wmain(int argc,wchar_t**argv){
 int failures=0;
 for(int i=1;i<argc;++i){try{
  std::filesystem::path folder=argv[i];auto read=[](auto const&p){std::ifstream f(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});};
  auto j=nlohmann::json::parse(read(folder/"manifest.json"));auto id=j.value("name",folder.filename().string());
  auto sidecar=folder/"modmenu.settings.json";auto definition=std::filesystem::exists(sidecar)?menu::ParseSettingsDescription(read(sidecar),id,j.at("entry").get<std::string>(),j.at("version").get<std::string>()):menu::SettingsSchema(id);
  std::cout<<id<<" "<<j.at("version").get<std::string>()<<" schema="<<definition.version<<" fields="<<definition.fields.size()<<"\n";
 }catch(std::exception const&e){++failures;std::cerr<<"Descriptor rejected: "<<e.what()<<'\n';}}
 return failures?1:0;
}
