#include "ModMenu/Settings.h"
#include "ModMenu/Storage.h"
#include "ModMenu/Slider.h"
#include "json.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdio>
#include <set>
#include <stdexcept>
namespace menu {
namespace {
namespace fs=std::filesystem;using Json=nlohmann::json;
struct Handle {
 HANDLE h{INVALID_HANDLE_VALUE};explicit Handle(HANDLE v):h(v){}
 ~Handle(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
 Handle(Handle const&)=delete;Handle& operator=(Handle const&)=delete;
};
struct Directories {
 std::vector<HANDLE> handles;
 ~Directories(){for(auto h:handles)CloseHandle(h);}
 bool Pin(fs::path p){
  p=fs::absolute(p).lexically_normal();
  for(;;){
   auto h=CreateFileW(p.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
   if(h==INVALID_HANDLE_VALUE)return false;
   handles.push_back(h);BY_HANDLE_FILE_INFORMATION info{};
   if(!GetFileInformationByHandle(h,&info) || !(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) || (info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT))return false;
   auto parent=p.parent_path();if(p==parent)break;p=parent;
  }return true;
 }
};
Json Parse(std::string const& bytes){
 if(!ValidUtf8(bytes))throw std::runtime_error("Invalid UTF-8 config");
 std::vector<std::set<std::string>> keys;
 auto cb=[&](int depth,Json::parse_event_t event,Json& value){
  if(depth>32)throw std::runtime_error("Config nesting limit");
  if(event==Json::parse_event_t::object_start)keys.emplace_back();
  if(event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)throw std::runtime_error("Duplicate config field");
  if(event==Json::parse_event_t::object_end)keys.pop_back();return true;
 };
 auto j=Json::parse(bytes,cb);if(!j.is_object())throw std::runtime_error("Config must be an object");return j;
}
std::string ReadHandle(HANDLE h,std::size_t limit){
 BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};
 if(h==INVALID_HANDLE_VALUE || !GetFileInformationByHandle(h,&info) ||
  (info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) || info.nNumberOfLinks!=1 ||
  !GetFileSizeEx(h,&size) || size.QuadPart<=0 || size.QuadPart>static_cast<LONGLONG>(limit))throw std::runtime_error("Config missing, unsafe, locked or too large");
 LARGE_INTEGER zero{};if(!SetFilePointerEx(h,zero,nullptr,FILE_BEGIN))throw std::runtime_error("Cannot read config");
 std::string bytes(static_cast<std::size_t>(size.QuadPart),'\0');DWORD used{};
 if(!ReadFile(h,bytes.data(),DWORD(bytes.size()),&used,nullptr) || used!=bytes.size())throw std::runtime_error("Cannot read config");return bytes;
}
std::string Read(fs::path const& path,std::size_t limit){
 Handle h(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));return ReadHandle(h.h,limit);
}
struct Sources {
 Handle manifest{INVALID_HANDLE_VALUE},description{INVALID_HANDLE_VALUE};
 SettingsDefinition schema;std::string manifestBytes,descriptionBytes,manifestFile;
 Sources(fs::path const& directory,std::string const& id){
  for(auto name:{"manifest.json","manifest.json.close","manifest.json.disabled"}){
   auto p=directory/name;if(GetFileAttributesW(p.c_str())==INVALID_FILE_ATTRIBUTES)continue;
   manifestFile=name;
   manifest.h=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
   manifestBytes=ReadHandle(manifest.h,65536);break;
  }
  if(manifestFile.empty())throw std::runtime_error("Installed plugin manifest unavailable");
  auto j=Parse(manifestBytes);auto sidecar=directory/"modmenu.settings.json";
  auto attributes=GetFileAttributesW(sidecar.c_str());
  if(attributes!=INVALID_FILE_ATTRIBUTES){
   description.h=CreateFileW(sidecar.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
   descriptionBytes=ReadHandle(description.h,65536);
   schema=ParseSettingsDescription(descriptionBytes,id,j.value("entry",std::string{}),j.value("version",std::string{}));
  }else{
   if(GetLastError()!=ERROR_FILE_NOT_FOUND)throw std::runtime_error("Settings description unavailable");
   schema=SettingsSchema(id);if(schema.fields.empty())throw std::runtime_error("No verified settings adapter");
   if(j.value("entry",std::string{})!=id+".dll")throw std::runtime_error("Plugin entry does not match adapter");
  }
 }
};
bool Valid(SettingField const& field){return ValidSettingValue(field);}
std::vector<SettingField> Fields(SettingsDefinition const& a,Json const& j,NumberTokens const& numbers){
 auto fields=a.fields;for(auto& f:fields){
  Json::json_pointer p(f.path);
  for(auto parent=p.parent_pointer();!parent.empty();parent=parent.parent_pointer())
   if(j.contains(parent) && !j[parent].is_object())throw std::runtime_error("Invalid setting group");
  if(j.contains(p)){
   auto const& v=j[p];
   if(f.type==SettingType::Boolean){if(!v.is_boolean())throw std::runtime_error("Expected boolean: "+f.label);f.value=v.get<bool>();}
   else if(f.type==SettingType::Key || f.type==SettingType::Text || f.type==SettingType::Binding){
     if(f.type==SettingType::Binding&&v.is_number_integer()&&v>=0&&v<=254){if(!SetBinding(f,{v.get<unsigned>(),0})||!Valid(f))throw std::runtime_error("Invalid legacy key");continue;}
    if(!f.external && ((f.type==SettingType::Key && f.keyFormat==KeyFormat::AutoJumpReset) || f.textFormat==TextFormat::Delay) && v.is_number_integer() && v>=0 && v<=(f.type==SettingType::Key?255:5000)){
     if(f.type==SettingType::Key){char token[16]{};std::snprintf(token,sizeof(token),"0x%02X",v.get<unsigned>());f.value=std::string(token);}else f.value=std::to_string(v.get<unsigned>());
    }else{
     if(!v.is_string())throw std::runtime_error("Expected key or text: "+f.label);
     auto s=v.get<std::string>();if(f.type==SettingType::Key && !f.external)for(auto& c:s)if(c>='a' && c<='z')c-=32;f.value=s;
    }
   }
   else if(f.type==SettingType::Slider){
    if(!v.is_number() || (!f.slider.decimal && !v.is_number_integer()))throw std::runtime_error("Invalid number: "+f.label);
    auto it=numbers.find(f.path);auto n=it==numbers.end()?std::optional<std::int64_t>{}:ParseFixed(it->second,f.slider.precision);
    if(!n)throw std::runtime_error("Invalid number: "+f.label);f.value=SliderValue{*n};
   }else {if(!v.is_number_integer() || v<f.minimum || v>f.maximum)throw std::runtime_error("Invalid number: "+f.label);f.value=v.get<int>();}
  }
  if(!Valid(f))throw std::runtime_error("Unsupported setting value: "+f.label);
 }return fields;
}
fs::path Unique(fs::path const& path,wchar_t const* extension){
 static std::atomic<unsigned long long> sequence{};
 auto p=path;p+=L".mmb-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L"-"+std::to_wstring(++sequence)+extension;return p;
}
void WriteNew(fs::path const& p,std::string const& bytes){
 Handle h(CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));DWORD used{};
 if(h.h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot create config transaction file");
 if(!WriteFile(h.h,bytes.data(),DWORD(bytes.size()),&used,nullptr) || used!=bytes.size() || !FlushFileBuffers(h.h)){
  CloseHandle(h.h);h.h=INVALID_HANDLE_VALUE;DeleteFileW(p.c_str());throw std::runtime_error("Cannot write config transaction file");
 }
}
void CleanupTemporary(fs::path const& p,bool created){if(created && !p.empty())DeleteFileW(p.c_str());}
}
bool HasSettings(std::string const& id){return !SettingsSchema(id).fields.empty();}
SettingsAvailability InspectSettings(fs::path const& mods,std::string const& id){
 try{
  if(!SafeSettingsId(id))throw std::runtime_error("Unsafe plugin directory");
  auto directory=fs::absolute(mods/id).lexically_normal();Directories pins;
  if(!pins.Pin(directory))throw std::runtime_error("Unsafe plugin directory");
  Sources source(directory,id);return {true,{}};
 }catch(std::exception const& e){return {false,e.what()};}
}
SettingsDraft LoadSettings(fs::path const& mods,std::string const& id){
 SettingsDraft result;result.id=id;
 try{
  if(!SafeSettingsId(id))throw std::runtime_error("Unsafe plugin directory");
  auto directory=fs::absolute(mods/id).lexically_normal();Directories pins;
  if(!pins.Pin(directory))throw std::runtime_error("Unsafe plugin directory");
  Sources source(directory,id);auto const& a=source.schema;result.original=Read(directory/a.file,a.limit);
  result.external=a.external;result.hotReload=a.hotReload;result.descriptionSource=source.descriptionBytes;result.manifestSource=source.manifestBytes;result.manifestFile=source.manifestFile;
  auto j=Parse(result.original);result.fields=Fields(a,j,ReadNumberTokens(result.original));result.valid=true;
  result.message=!a.external?"Apply saves JSON; live application is not confirmed. Disabled DLLs need restart.":
   a.hotReload?"Apply saves JSON; the plugin declares hot reload. Application is not acknowledged.":"Apply saves JSON; restart Minecraft for these settings to take effect.";
 }catch(std::exception const& e){result.message=e.what();}return result;
}
SettingsDraft ApplySettings(fs::path const& mods,SettingsDraft const& draft){
 SettingsDraft result=draft;result.saved=false;fs::path temporary;bool temporaryCreated=false;
 try{
  if(!SafeSettingsId(draft.id))throw std::runtime_error("Unsafe plugin directory");
  auto directory=fs::absolute(mods/draft.id).lexically_normal();Directories pins;
  if(!pins.Pin(directory))throw std::runtime_error("Unsafe plugin directory");
  Sources sources(directory,draft.id);auto const& a=sources.schema;
  if(draft.external!=a.external || draft.hotReload!=a.hotReload || draft.descriptionSource!=sources.descriptionBytes || draft.manifestSource!=sources.manifestBytes || draft.manifestFile!=sources.manifestFile)
   throw std::runtime_error("Settings description or manifest changed. Cancel and reopen Configure.");
  if(!draft.valid || a.fields.empty() || draft.fields.size()!=a.fields.size())throw std::runtime_error("Invalid settings draft");
  auto j=Parse(draft.original);auto expected=Fields(a,j,ReadNumberTokens(draft.original));NumberTokens outputNumbers;
  for(std::size_t i=0;i<expected.size();++i){auto f=expected[i];auto const& change=draft.fields[i];
   if(f.path!=change.path || f.type!=change.type)throw std::runtime_error("Unexposed setting edit refused");
   f.value=change.value;if(!Valid(f))throw std::runtime_error("Invalid value: "+f.label);
   // Preserve absent defaults and unedited native number/string forms.
   if(f.value!=expected[i].value){
    if(f.type==SettingType::Text && !SetText(f,std::get<std::string>(f.value)))throw std::runtime_error("Invalid value: "+f.label);
    if(f.type==SettingType::Slider)j[Json::json_pointer(f.path)]=0;
    else std::visit([&](auto const& v){if constexpr(!std::is_same_v<std::decay_t<decltype(v)>,SliderValue>)j[Json::json_pointer(f.path)]=v;},f.value);
   }
   if(f.type==SettingType::Slider && j.contains(Json::json_pointer(f.path)))outputNumbers[f.path]=FormatFixed(std::get<SliderValue>(f.value).ticks,f.slider.precision);
  }
  auto bytes=DumpNumbers(j,outputNumbers)+"\n";if(bytes.size()>a.limit)throw std::runtime_error("Result exceeds plugin config limit");
  auto path=directory/a.file;Handle source(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
  if(ReadHandle(source.h,a.limit)!=draft.original)throw std::runtime_error("Config changed externally. Cancel and reopen Configure.");
  BY_HANDLE_FILE_INFORMATION info{};if(!GetFileInformationByHandle(source.h,&info) || (info.dwFileAttributes&FILE_ATTRIBUTE_READONLY))throw std::runtime_error("Config is read-only");
  temporary=Unique(path,L".tmp");WriteNew(temporary,bytes);temporaryCreated=true;
  auto backup=Unique(path,L".bak");WriteNew(backup,draft.original);
  // Optimistic check also detects an external atomic replacement while the
  // source read handle excludes in-place writes. Ancestor handles deny rename.
  Handle current(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
  BY_HANDLE_FILE_INFORMATION now{};
  if(!GetFileInformationByHandle(current.h,&now) || now.dwVolumeSerialNumber!=info.dwVolumeSerialNumber ||
   now.nFileIndexHigh!=info.nFileIndexHigh || now.nFileIndexLow!=info.nFileIndexLow || ReadHandle(current.h,a.limit)!=draft.original)
   throw std::runtime_error("Config replaced externally. Cancel and reopen Configure.");
  if(!a.external && GetFileAttributesW((directory/"modmenu.settings.json").c_str())!=INVALID_FILE_ATTRIBUTES)
   throw std::runtime_error("Settings description or manifest changed. Cancel and reopen Configure.");
  if(!ReplaceFileW(path.c_str(),temporary.c_str(),nullptr,0,nullptr,nullptr))throw std::runtime_error("Atomic save failed; exact backup retained. Cancel and inspect the current config before retrying.");
  temporary.clear();result.original=bytes;result.saved=true;
  result.message=!a.external?"Saved with backup. Live reload expected, not confirmed; disabled DLLs need restart.":
   a.hotReload?"Saved with backup. The plugin declares hot reload; application is not acknowledged.":"Saved with backup. Restart Minecraft for these settings to take effect.";
 }catch(std::exception const& e){result.message=e.what();}
 CleanupTemporary(temporary,temporaryCreated);return result;
}
std::string SettingText(SettingField const& field){
 if(auto n=std::get_if<SliderValue>(&field.value))return FormatFixed(n->ticks,field.slider.precision);
 if(auto b=std::get_if<bool>(&field.value))return *b?"ON":"OFF";
 if(auto n=std::get_if<int>(&field.value)){
  if(field.type==SettingType::Choice && *n>=0 && std::size_t(*n)<field.options.size())return field.options[*n];return std::to_string(*n);
 }return std::get<std::string>(field.value);
}
bool SetKey(SettingField& field,unsigned key){
 if(field.type==SettingType::Binding)return SetBinding(field,{key==27?0u:key,0});
 if(field.type!=SettingType::Key)return false;std::string name;
 if(!field.external && field.keyFormat!=KeyFormat::Menu){
  if(key==27)key=0;
  else if(key<8 || key>=255 || (key>=91 && key<=93))return false;
  char token[16]{};std::snprintf(token,sizeof(token),"0x%02X",key);auto copy=field;copy.value=std::string(token);
  if(!Valid(copy))return false;field.value=copy.value;return true;
 }
 if((key>='A' && key<='Z') || (key>='0' && key<='9'))name=char(key);
 else if(key>=112 && key<=135)name="F"+std::to_string(key-111);
 else switch(key){case 0xA4:name="LALT";break;case 0xA5:name="RALT";break;case 0xA2:name="LCTRL";break;case 0xA3:name="RCTRL";break;case 0xA0:name="LSHIFT";break;case 0xA1:name="RSHIFT";break;case 32:name="SPACE";break;case 45:name="INSERT";break;case 46:name="DELETE";break;case 36:name="HOME";break;case 35:name="END";break;case 33:name="PAGEUP";break;case 34:name="PAGEDOWN";break;case 19:name="PAUSE";break;case 27:case 8:name="NONE";break;default:return false;}
 if(std::find(field.options.begin(),field.options.end(),name)==field.options.end())return false;field.value=name;return true;
}
}
