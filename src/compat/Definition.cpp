#include "ModMenu/Definition.h"
#include "json.hpp"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <set>
#include <stdexcept>
#include <vector>
namespace menu {
std::string_view VerifiedMenu60(std::string_view source){
 char const* expected=nullptr;std::string_view target;
 if(source.size()==48127){target="start";expected="5F393CD5869DE6B241C91A2B6189551762017FA71CD04CB3097521A28C987813";}
 else if(source.size()==48802){target="pause";expected="07D478EE18CA1A7D83897A36353777B97AFDD876EAFD9AFCA7093A9E69EE72BC";}
 else return {};
 BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE handle{};DWORD size{},used{};
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
 bool ok=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),4,&used,0)>=0;
 std::vector<unsigned char> object(ok?size:0);std::array<unsigned char,32> digest{};
 if(ok)ok=BCryptCreateHash(algorithm,&handle,object.data(),size,nullptr,0,0)>=0;
 if(ok)ok=BCryptHashData(handle,reinterpret_cast<PUCHAR>(const_cast<char*>(source.data())),ULONG(source.size()),0)>=0 && BCryptFinishHash(handle,digest.data(),32,0)>=0;
 if(handle)BCryptDestroyHash(handle);BCryptCloseAlgorithmProvider(algorithm,0);
 constexpr char hex[]="0123456789ABCDEF";
 for(unsigned i=0;ok && i<32;++i)ok=hex[digest[i]>>4]==expected[i*2] && hex[digest[i]&15]==expected[i*2+1];
 return ok?target:std::string_view{};
}
namespace {
using Json=nlohmann::ordered_json;
constexpr std::size_t Limit=1024*1024;
Json Button(){
 return {{"$button_text","Mods"},{"$pressed_button_name",ModsEventName},{"$focus_id","mmb_mods_button"},
  {"button_mappings",Json::array({
   {{"from_button_id","button.menu_select"},{"to_button_id","$pressed_button_name"},{"mapping_type","pressed"}},
   {{"from_button_id","button.menu_ok"},{"to_button_id","$pressed_button_name"},{"mapping_type","pressed"}}})}};
}
Json Row(){return {{"mmb_mods@start.stacked_row",{{"controls",Json::array({{{"mmb_button@start.mmb_button",Json::object()}}})}}}};}
bool InsertPause(Json& j,std::string const& source,std::string& output){
 if(!j.contains("pause_button_template@common_buttons.light_text_button") ||
  !j["pause_button_template@common_buttons.light_text_button"].is_object() ||
  !j.contains("settings_button@pause.pause_button_template") ||
  j["settings_button@pause.pause_button_template"].value("$pressed_button_name",std::string{})!="button.menu_settings" ||
  !j.contains("menu_background@pause.transparent_background"))return false;
 auto& background=j["menu_background@pause.transparent_background"];
 if(!background.is_object() || !background.contains("controls") || !background["controls"].is_array())return false;
 Json* rows=nullptr;
 for(auto& entry:background["controls"]){
  if(!entry.is_object() || entry.size()!=1)return false;
  if(!entry.contains("button_panel"))continue;
  auto& stack=entry["button_panel"];
  if(rows || !stack.is_object() || stack.value("type",std::string{})!="stack_panel" ||
   stack.value("orientation",std::string("vertical"))!="vertical" || !stack.contains("controls") || !stack["controls"].is_array())return false;
  rows=&stack["controls"];
 }
 if(!rows || rows->size()>256)return false;
 std::size_t settings=rows->size(),resume=rows->size(),quit=rows->size(),existing=rows->size();
 unsigned settingsCount{},resumeCount{},quitCount{},mods{};
 for(std::size_t i=0;i<rows->size();++i){auto const& row=(*rows)[i];if(!row.is_object() || row.size()!=1)return false;
  auto const& key=row.begin().key();
  if(key=="settings_button@pause.settings_button"){settings=i;++settingsCount;}
  if(key=="return_to_game_button@pause.return_to_game_button"){resume=i;++resumeCount;}
  if(key=="quit_button@pause.quit_button"){quit=i;++quitCount;}
  if(key.starts_with("mmb_")){if(existing==rows->size())existing=i;++mods;}
 }
 if(settingsCount!=1 || resumeCount!=1 || quitCount!=1 || resume>=settings || settings>=quit || settings+1>=rows->size() ||
  !(*rows)[settings+1].contains("settings@pause.vertical_padding"))return false;
 auto button=Button();button["focus_identifier"]="mmb_pause_mods_button";
 button["focus_change_left"]="FOCUS_OVERRIDE_STOP";button["focus_change_right"]="friendsdrawer_button";
 button["anchor_from"]="top_middle";button["anchor_to"]="top_middle";
 Json row={{"mmb_mods@pause.mmb_button",Json::object()}};
 Json spacing={{"mmb_spacing@pause.vertical_padding",{{"size",Json::array({0,3})}}}};
 constexpr char own[]="mmb_button@pause.pause_button_template";
 for(auto const& item:j.items())if(item.key().starts_with("mmb_") && item.key()!=own)return false;
 if(mods || j.contains(own)){
  if(mods!=2 || existing!=settings+2 || existing+1>=rows->size() || (*rows)[existing]!=row ||
   (*rows)[existing+1]!=spacing || !j.contains(own) || j[own]!=button)return false;
  output=source;return true;
 }
 auto at=rows->begin()+static_cast<Json::difference_type>(settings+2);
 at=rows->insert(at,row);rows->insert(at+1,spacing);j[own]=std::move(button);
 auto result=j.dump();if(result.size()>Limit)return false;output=std::move(result);return true;
}
}
std::string_view DefinitionNamespace(std::string_view path){
 if(path=="ui/start_screen.json")return "start";
 if(path=="ui/pause_screen.json")return "pause";
 return {};
}
bool InsertModsDefinition(std::string const& source,std::string& output,std::string_view expectedNamespace){
 try{
  if(source.empty() || source.size()>Limit)return false;
  std::vector<std::set<std::string>> keys;
  auto callback=[&](int depth,Json::parse_event_t event,Json& value){
   if(depth>64)throw std::runtime_error("UI definition nesting limit");
   if(event==Json::parse_event_t::object_start)keys.emplace_back();
   if(event==Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)throw std::runtime_error("Duplicate UI key");
   if(event==Json::parse_event_t::object_end)keys.pop_back();return true;
  };
  auto j=Json::parse(source,callback,true,true);
  if(!expectedNamespace.empty() && (!j.is_object() || j.value("namespace",std::string{})!=expectedNamespace))return false;
  // A pack-managed entry owns its placement and style. This reserved control
  // was already refused by the legacy mmb_ collision rule. Keep that behavior
  // explicit for the public contract; never overwrite or validate its design.
  if(j.is_object() && j.contains(ResourcePackEntryMarker))return false;
  if(j.is_object() && j.value("namespace",std::string{})=="pause")return InsertPause(j,source,output);
  if(!j.is_object() || j.value("namespace",std::string{})!="start" || !j.contains("stacked_row") ||
   !j["stacked_row"].is_object() || j["stacked_row"].value("type",std::string{})!="panel")return false;
  unsigned bases{};for(auto const& item:j.items())if(item.key().starts_with("main_button@") && item.value().is_object())++bases;
  if(bases!=1 || !j.contains("main_buttons_panel"))return false;
  auto& panel=j["main_buttons_panel"];if(!panel.is_object() || !panel.contains("controls") || !panel["controls"].is_array())return false;
  Json* rows=nullptr;
  for(auto& item:panel["controls"]){
   if(!item.is_object() || item.size()!=1)return false;
   if(!item.contains("stacked_rows"))continue;
   auto& stack=item["stacked_rows"];
   if(rows || !stack.is_object() || stack.value("type",std::string{})!="stack_panel" || !stack.contains("controls") || !stack["controls"].is_array())return false;
   rows=&stack["controls"];
  }
  if(!rows || rows->size()>256)return false;
  std::size_t play=rows->size(),settingsRow=rows->size(),existing=rows->size();unsigned plays{},mods{},settings{};
  for(std::size_t i=0;i<rows->size();++i){auto const& row=(*rows)[i];if(!row.is_object() || row.size()!=1)return false;
   auto const& key=row.begin().key();
   if(key=="play@start.play_button_stack"){play=i;++plays;}
   if(key=="settings@start.stacked_row"){settingsRow=i;++settings;}
   if(key.starts_with("mmb_")){existing=i;++mods;}
  }
  if(plays!=1 || settings!=1 || play>=settingsRow)return false;
  auto button=Button();auto row=Row();
  for(auto const& item:j.items())if(item.key().starts_with("mmb_") && item.key()!="mmb_button@start.main_button")return false;
  if(mods || j.contains("mmb_button@start.main_button")){
   if(mods!=1 || existing!=play+1 || (*rows)[existing]!=row || !j.contains("mmb_button@start.main_button") || j["mmb_button@start.main_button"]!=button)return false;
   output=source;return true;
  }
  rows->insert(rows->begin()+static_cast<Json::difference_type>(play+1),row);
  j["mmb_button@start.main_button"]=std::move(button);
  auto result=j.dump();if(result.size()>Limit)return false;output=std::move(result);return true;
 }catch(...){return false;}
}
}
