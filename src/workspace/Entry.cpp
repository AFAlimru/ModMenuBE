#include "Entry.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace modmenu::workspace {
namespace {
void need(bool value,char const* text){if(!value)throw std::runtime_error(text);}
void members(nlohmann::json const& j,std::initializer_list<char const*> allowed){need(j.is_object(),"Expected object");for(auto const&[k,v]:j.items()){(void)v;need(std::any_of(allowed.begin(),allowed.end(),[&](auto p){return k==p;}),"Unknown workspace property");}}
std::string id(nlohmann::json const& j){need(j.is_string(),"Expected target ID");auto s=j.get<std::string>();need(identifier(s),"Invalid target ID");return s;}
}
bool identifier(std::string_view s){if(s.empty()||s.size()>64)return false;return std::all_of(s.begin(),s.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='-';});}
bool validText(std::string_view s,std::size_t max,bool empty){
 if((s.empty()&&!empty)||s.size()>max)return false;
 for(std::size_t i=0;i<s.size();){unsigned char c=s[i++];if(c<32||c==127)return false;if(c<128)continue;unsigned count=c>=0xC2&&c<=0xDF?1:c>=0xE0&&c<=0xEF?2:c>=0xF0&&c<=0xF4?3:0;if(!count||i+count>s.size())return false;unsigned char next=s[i];if((c==0xE0&&next<0xA0)||(c==0xED&&next>=0xA0)||(c==0xF0&&next<0x90)||(c==0xF4&&next>=0x90))return false;for(unsigned n=0;n<count;++n)if((static_cast<unsigned char>(s[i++])&0xC0)!=0x80)return false;}
 return s.find("\xc2\xa7")==std::string_view::npos;
}
Caption parseCaption(nlohmann::json const& j,std::size_t max){Caption c;if(j.is_string()){auto s=j.get<std::string>();need(validText(s,max),"Invalid workspace label");c.values["en_US"]=s;}else{need(j.is_object()&&j.contains("en_US")&&j.size()<=16,"Label needs English fallback");for(auto const&[lang,text]:j.items()){need(lang.size()<=16&&text.is_string(),"Invalid label translation");auto s=text.get<std::string>();need(validText(s,max),"Invalid label text");c.values[lang]=s;}}return c;}
std::string Caption::get(std::string const& lang)const{if(auto i=values.find(lang);i!=values.end())return i->second;if(auto i=values.find("en_US");i!=values.end())return i->second;return values.empty()?std::string():values.begin()->second;}
EntryDefinition parseEntry(std::string const& raw){EntryDefinition result;try{
 need(!raw.empty()&&raw.size()<=1048576,"Adapter exceeds size limit");std::vector<std::set<std::string>> stack;
 auto j=nlohmann::json::parse(raw,[&](int depth,nlohmann::json::parse_event_t event,nlohmann::json& value){need(depth<=32,"Adapter nesting limit");if(event==nlohmann::json::parse_event_t::object_start)stack.emplace_back();if(event==nlohmann::json::parse_event_t::key)need(!stack.empty()&&stack.back().insert(value.get<std::string>()).second,"Duplicate adapter member");if(event==nlohmann::json::parse_event_t::object_end)stack.pop_back();return true;},true,true);
 need(j.is_object(),"Expected adapter object");if(!j.contains("workspace")||j["workspace"]==false)return result;result.declared=true;
 auto const&w=j.at("workspace");members(w,{"version","label","entries"});need(w.at("version").is_number_integer()&&w.at("version")==1,"Unsupported workspace entry version");result.label=parseCaption(w.value("label",nlohmann::json("Workspace")));
 auto const& entries=w.at("entries");need(entries.is_array()&&!entries.empty()&&entries.size()<=16,"Expected 1-16 workspace entries");std::set<std::string> ids;
 for(auto const& e:entries){members(e,{"id","type","target","label"});Entry entry;entry.id=id(e.at("id"));need(ids.insert(entry.id).second,"Duplicate workspace entry ID");entry.target=id(e.at("target"));entry.label=parseCaption(e.at("label"));auto type=e.at("type").get<std::string>();need(type=="page"||type=="external","Unknown workspace entry type");entry.external=type=="external";result.entries.push_back(std::move(entry));}result.valid=true;
 }catch(std::exception const&){result.valid=false;result.entries.clear();result.error="Invalid workspace declaration (version, IDs, labels or bounds).";}return result;}
}
