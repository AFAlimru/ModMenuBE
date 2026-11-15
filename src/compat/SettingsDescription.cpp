#include "ModMenu/Settings.h"
#include "ModMenu/Storage.h"
#include "ModMenu/Slider.h"
#include "json.hpp"
#include <algorithm>
#include <cctype>
#include <climits>
#include <set>
#include <stdexcept>
namespace menu {
namespace {
using Json=nlohmann::json;
void Require(bool condition,char const* message){if(!condition)throw std::runtime_error(message);}
void Members(Json const& j,std::initializer_list<char const*> allowed){
 Require(j.is_object(),"Description section must be an object");
 for(auto const& [key,value]:j.items()){
  (void)value;Require(std::any_of(allowed.begin(),allowed.end(),[&](auto a){return key==a;}),"Unknown description property");
 }
}
std::string Text(Json const& j,std::size_t limit,bool lines=false){
 Require(j.is_string(),"Description text must be a string");auto s=j.get<std::string>();
 Require(!s.empty() && s.size()<=limit && ValidUtf8(s) && s.find("\xc2\xa7")==std::string::npos,"Invalid description text");
 for(unsigned char c:s)Require((c>=32 && c!=127) || (lines && (c=='\n' || c=='\r')),"Invalid description text control");
 return s;
}
SettingTranslation Caption(Json const& j,std::size_t limit,bool lines=false){
 if(j.is_string())return {Text(j,limit,lines),{},{} };
 Members(j,{"en_US","zh_TW","zh_CN"});Require(j.contains("en_US"),"English description fallback is required");
 SettingTranslation t{Text(j.at("en_US"),limit,lines),{},{} };
 if(j.contains("zh_TW"))t.tw=Text(j.at("zh_TW"),limit,lines);
 if(j.contains("zh_CN"))t.cn=Text(j.at("zh_CN"),limit,lines);return t;
}
int Integer(Json const& j){Require(j.is_number_integer() && j>=0 && j<=INT_MAX,"Invalid description integer");return j.get<int>();}
bool Basename(std::string const& name,char const* extension){
 if(name.size()<=std::string_view(extension).size() || name.size()>120 || name.front()=='.' || !name.ends_with(extension))return false;
 for(unsigned char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;
 auto stem=name.substr(0,name.find('.'));for(char& c:stem)c=char(std::toupper(static_cast<unsigned char>(c)));
 if(stem=="CON" || stem=="PRN" || stem=="AUX" || stem=="NUL" ||
    (stem.size()==4 && (stem.starts_with("COM") || stem.starts_with("LPT")) && stem[3]>='0' && stem[3]<='9'))return false;
 return name.find("..") == std::string::npos;
}
bool Key(std::string const& key){
 if(key.size()==1 && ((key[0]>='A' && key[0]<='Z') || (key[0]>='0' && key[0]<='9')))return true;
 for(int n=1;n<=24;++n)if(key=="F"+std::to_string(n))return true;
 for(auto name:{"NONE","INSERT","DELETE","HOME","END","PAGEUP","PAGEDOWN","PAUSE","LALT","RALT","LCTRL","RCTRL","LSHIFT","RSHIFT","SPACE"})if(key==name)return true;
 return false;
}
void Pointer(std::string const& path){
 Require(path.size()>=2 && path.size()<=256 && path.front()=='/',"Invalid setting pointer");
 unsigned count{};std::size_t begin=1;
 while(begin<=path.size()){
  auto end=path.find('/',begin);if(end==std::string::npos)end=path.size();
  auto token=path.substr(begin,end-begin);Require(!token.empty() && ++count<=16,"Invalid setting pointer depth");
  std::string decoded;
  for(std::size_t i=0;i<token.size();++i){if(token[i]=='~'){Require(i+1<token.size() && (token[i+1]=='0' || token[i+1]=='1'),"Invalid setting pointer escape");decoded+=token[++i]=='0'?'~':'/';}else decoded+=token[i];}
  Require(!std::all_of(decoded.begin(),decoded.end(),[](unsigned char c){return c>='0' && c<='9';}),"Array setting pointer is unsupported");
  if(end==path.size())break;begin=end+1;
 }
}
}
bool SafeSettingsId(std::string_view id){
 if(id.empty() || id.size()>120 || id=="." || id=="..")return false;
 for(unsigned char c:id)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'))return false;
 return true;
}
SettingsDefinition ParseSettingsDescription(std::string const& bytes,std::string const& id,std::string const& entry,std::string const& version){
 Require(!bytes.empty() && bytes.size()<=65536 && ValidUtf8(bytes),"Invalid settings description size or UTF-8");
 std::vector<std::set<std::string>> keys;
 auto callback=[&](int depth,Json::parse_event_t event,Json& value){
  Require(depth<=32,"Description nesting limit");
  if(event==Json::parse_event_t::object_start)keys.emplace_back();
  if(event==Json::parse_event_t::key)Require(keys.back().insert(value.get<std::string>()).second,"Duplicate description field");
  if(event==Json::parse_event_t::object_end)keys.pop_back();return true;
 };
 auto j=Json::parse(bytes,callback);Members(j,{"$schema","schemaVersion","plugin","config","fields"});
 Require(j.contains("schemaVersion"),"Unsupported settings description version");auto schemaVersion=Integer(j.at("schemaVersion"));
 Require(schemaVersion>=1 && schemaVersion<=5,"Unsupported settings description version");
 auto numbers=schemaVersion>=2?ReadNumberTokens(bytes):NumberTokens{};
 if(j.contains("$schema"))(void)Text(j.at("$schema"),512);
 auto const& plugin=j.at("plugin");Members(plugin,{"id","entry","versions"});
 Require(SafeSettingsId(id) && Text(plugin.at("id"),120)==id,"Description plugin ID mismatch");
 auto expected=Text(plugin.at("entry"),120);Require(Basename(expected,".dll") && expected==entry,"Description plugin entry mismatch");
 auto const& versions=plugin.at("versions");Require(versions.is_array() && !versions.empty() && versions.size()<=32,"Invalid description versions");
 std::set<std::string> accepted;bool matches=false;
 for(auto const& v:versions){auto s=Text(v,80);Require(s.find('*')==std::string::npos && accepted.insert(s).second,"Invalid exact description version");matches|=s==version;}
 Require(matches,"Description plugin version mismatch");
 auto const& config=j.at("config");Members(config,{"file","reload"});auto file=Text(config.at("file"),120);
 std::string lower=file;for(auto& c:lower)c=char(std::tolower(static_cast<unsigned char>(c)));
 Require(Basename(file,".json") && lower!="manifest.json" && lower!="mod.json" && lower!="modmenu.settings.json" && lower!="config.schema.json","Unsafe description config filename");
 auto reload=Text(config.at("reload"),16);Require(reload=="hot" || reload=="restart","Invalid description reload policy");
 SettingsDefinition result;result.version=unsigned(schemaVersion);result.external=true;result.file=file;result.limit=65536;result.hotReload=reload=="hot";
 auto const& fields=j.at("fields");Require(fields.is_array() && !fields.empty() && fields.size()<=64,"Invalid description fields");
 std::set<std::string> paths;
 std::size_t fieldIndex{};
 for(auto const& raw:fields){
  if(schemaVersion==1)Members(raw,{"path","type","default","label","category","description","min","max","step","unit","options","keys"});
  else if(schemaVersion==2)Members(raw,{"path","type","default","label","category","description","min","max","step","unit","options","keys","numberType","precision"});
  else if(schemaVersion==3)Members(raw,{"path","type","default","label","category","description","min","max","step","unit","options","keys","numberType","precision","minBytes","maxBytes","validation"});
  else Members(raw,{"path","type","default","label","category","description","min","max","step","unit","options","keys","numberType","precision","minBytes","maxBytes","validation","legacyNumericStrings"});
  SettingField f;f.external=true;f.path=Text(raw.at("path"),256);Pointer(f.path);
  for(auto const& p:paths)Require(p!=f.path && !p.starts_with(f.path+"/") && !f.path.starts_with(p+"/"),"Overlapping description fields");paths.insert(f.path);
  f.labelText=Caption(raw.at("label"),160);f.label=f.labelText.en;
  if(raw.contains("category")){f.categoryText=Caption(raw.at("category"),80);f.category=f.categoryText.en;Require(f.category!="All","Reserved description category");}
  if(raw.contains("description")){f.descriptionText=Caption(raw.at("description"),1024,true);f.description=f.descriptionText.en;}
  auto type=Text(raw.at("type"),16);auto const& value=raw.at("default");
  if(type=="boolean"){f.type=SettingType::Boolean;Require(value.is_boolean(),"Invalid Boolean description default");f.value=value.get<bool>();}
  else if(type=="integer"){
   f.type=SettingType::Integer;f.minimum=Integer(raw.at("min"));f.maximum=Integer(raw.at("max"));
   Require(f.minimum<=f.maximum,"Invalid description numeric range");f.value=Integer(value);
   if(raw.contains("step")){f.step=Integer(raw.at("step"));Require(f.step>0,"Invalid description numeric step");}
   if(raw.contains("unit"))f.unit=Text(raw.at("unit"),16);
  }else if(type=="text" && schemaVersion>=3){
   f.type=SettingType::Text;f.textFormat=TextFormat::Public;f.minBytes=Integer(raw.at("minBytes"));f.maxBytes=Integer(raw.at("maxBytes"));
   Require(f.minBytes<=f.maxBytes&&f.maxBytes>0&&f.maxBytes<=4096,"Invalid text byte limits");
   if(raw.contains("validation")){auto validation=Text(raw.at("validation"),16);Require(validation=="none"||validation=="identifier","Unsupported text validation");f.identifier=validation=="identifier";}
   Require(value.is_string(),"Text default must be a string");f.value=value.get<std::string>();Require(ValidSettingValue(f),"Invalid text default");
  }else if(type=="slider" && schemaVersion>=2){
   f.type=SettingType::Slider;auto kind=Text(raw.at("numberType"),16);Require(kind=="integer" || kind=="decimal","Invalid slider number type");f.slider.decimal=kind=="decimal";
   if(f.slider.decimal){f.slider.precision=unsigned(Integer(raw.at("precision")));Require(f.slider.precision>=1 && f.slider.precision<=6,"Invalid slider precision");}
   else Require(!raw.contains("precision"),"Integer slider must omit precision");
   auto fixed=[&](char const* member){
    Require(raw.at(member).is_number() && (f.slider.decimal || raw.at(member).is_number_integer()),"Invalid slider numeric type");
    auto it=numbers.find("/fields/"+std::to_string(fieldIndex)+"/"+member);Require(it!=numbers.end(),"Missing slider number");
    auto n=ParseFixed(it->second,f.slider.precision);Require(n.has_value(),"Invalid slider numeric precision or range");return *n;
   };
   f.slider.minimum=fixed("min");f.slider.maximum=fixed("max");f.slider.step=fixed("step");f.value=SliderValue{fixed("default")};
   if(raw.contains("unit"))f.unit=Text(raw.at("unit"),16);Require(ValidSlider(f),"Invalid slider range step or default");
  }else if(type=="choice"){
   f.type=SettingType::Choice;auto const& options=raw.at("options");Require(options.is_array() && options.size()>=2 && options.size()<=6,"Invalid description choice options");
   std::set<std::string> captions;for(auto const& option:options){auto text=Caption(option,160);Require(captions.insert(text.en).second,"Duplicate description choice caption");f.options.push_back(text.en);f.optionText.push_back(std::move(text));}
   f.maximum=int(f.options.size())-1;f.value=Integer(value);
  }else if(type=="binding" && schemaVersion>=4){
   f.type=SettingType::Binding;f.bindingModifiers=schemaVersion>=5?modmenu::binding::AllModifiers:3;auto name=Text(value,modmenu::binding::MaxBytes);
   if(raw.contains("legacyNumericStrings")){Require(raw["legacyNumericStrings"].is_boolean(),"Invalid binding legacy encoding");f.legacyNumericStrings=raw["legacyNumericStrings"].get<bool>();}
    auto binding=modmenu::binding::Parse(name);Require(binding.has_value()&&!(binding->modifiers&~f.bindingModifiers)&&modmenu::binding::Token(*binding)==name,"Invalid binding default");f.value=name;if(f.legacyNumericStrings)Require(SetBinding(f,*binding),"Invalid legacy binding default");
  }else if(type=="key"){
   f.type=SettingType::Key;auto const& list=raw.at("keys");Require(list.is_array() && !list.empty() && list.size()<=96,"Invalid description keys");
   std::set<std::string> tokens;for(auto const& k:list){auto name=Text(k,16);Require(Key(name) && tokens.insert(name).second,"Invalid description key token");f.options.push_back(name);}
   auto name=Text(value,16);Require(tokens.contains(name),"Invalid description key default");f.value=name;
  }else throw std::runtime_error("Unsupported description setting type");
  for(auto name:{"min","max","step","unit"})Require(!raw.contains(name) || f.type==SettingType::Integer || f.type==SettingType::Slider,"Description property does not match type");
  for(auto name:{"numberType","precision"})Require(!raw.contains(name) || f.type==SettingType::Slider,"Description property does not match type");
  Require(!raw.contains("options") || f.type==SettingType::Choice,"Description property does not match type");
  Require(!raw.contains("keys") || f.type==SettingType::Key,"Description property does not match type");
  Require(!raw.contains("legacyNumericStrings") || f.type==SettingType::Binding,"Binding encoding requires a binding field");
  for(auto name:{"minBytes","maxBytes","validation"})Require(!raw.contains(name)||f.type==SettingType::Text,"Description property does not match text");
  if(auto n=std::get_if<int>(&f.value))Require(*n>=f.minimum && *n<=f.maximum,"Description default outside range");
  f.defaultValue=f.value;result.fields.push_back(std::move(f));++fieldIndex;
 }
 // A category identity has one translation, independent of selected field.
 for(auto const& a:result.fields)for(auto const& b:result.fields)if(a.category==b.category)
  Require(a.categoryText.en==b.categoryText.en && a.categoryText.tw==b.categoryText.tw && a.categoryText.cn==b.categoryText.cn,"Inconsistent description category translation");
 return result;
}
}
