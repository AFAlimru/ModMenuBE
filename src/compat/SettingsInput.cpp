#include "ModMenu/Settings.h"
#include "ModMenu/Slider.h"
#include "ModMenu/TextEditor.h"
#include <algorithm>
#include <charconv>
#include <array>
#include <optional>
namespace menu {
namespace {
std::string_view Trim(std::string_view s){while(!s.empty() && s.front()==' ')s.remove_prefix(1);while(!s.empty() && s.back()==' ')s.remove_suffix(1);return s;}
std::string Upper(std::string_view s){std::string r(s);for(auto& c:r)if(c>='a' && c<='z')c-=32;return r;}
std::optional<unsigned> Number(std::string_view text,unsigned limit,bool nativeBase=false){
 text=Trim(text);if(text.empty())return {};
 int base=10;
 if(nativeBase && text.size()>2 && text[0]=='0' && (text[1]=='x' || text[1]=='X')){base=16;text.remove_prefix(2);}
 else if(nativeBase && text.size()>1 && text[0]=='0')base=8;
 unsigned n{};auto parsed=std::from_chars(text.data(),text.data()+text.size(),n,base);
 if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || n>limit)return {};return n;
}
std::optional<unsigned> NativeKey(SettingField const& field,std::string_view text){
 text=Trim(text);if(text.empty())return {};
 // Both old parsers try strtoul(base 0) before their one-character case.
 if(text.front()>='0' && text.front()<='9'){
  auto n=Number(text,255,true);if(!n)return {};return n;
 }
 auto key=Upper(text);
 if(key=="NONE")return 0;
 if(key.size()==1){
  auto c=key[0];if(c>='A' && c<='Z')return unsigned(c);
  if(c=='-' || c=='+' || c=='=')return c=='-'?0xBDu:0xBBu;
  if(field.keyFormat==KeyFormat::AutoJumpReset && (c=='[' || c==']'))return c=='['?0xDBu:0xDDu;
  return {};
 }
 if(key.front()=='F')if(auto n=Number(std::string_view(key).substr(1),24);n && *n)return 111+*n;
 for(auto [name,code]:{std::pair{"SPACE",32u},{"ENTER",13u},{"ESC",27u},{"ESCAPE",27u},{"TAB",9u}})if(key==name)return code;
 if(field.keyFormat==KeyFormat::AutoJumpReset){
  for(auto [name,code]:{std::pair{"INS",45u},{"INSERT",45u},{"DEL",46u},{"DELETE",46u},{"SHIFT",16u},
      {"CTRL",17u},{"CONTROL",17u},{"ALT",18u},{"LBUTTON",1u},{"RBUTTON",2u},{"MBUTTON",4u},
      {"XBUTTON1",5u},{"XBUTTON2",6u},{"UP",38u},{"DOWN",40u},{"LEFT",37u},{"RIGHT",39u}})if(key==name)return code;
 }return {};
}
std::optional<unsigned> MapKey(std::string_view token){
 auto key=Upper(token);if(key.size()==1 && ((key[0]>='A' && key[0]<='Z') || (key[0]>='0' && key[0]<='9')))return unsigned(key[0]);
 for(auto [name,code]:{std::pair{"ESC",27u},{"ESCAPE",27u},{"TAB",9u},{"SPACE",32u},{"ENTER",13u},
     {"MOUSE4",5u},{"MB4",5u},{"XBUTTON1",5u},{"MOUSE5",6u},{"MB5",6u},{"XBUTTON2",6u}})if(key==name)return code;
 if(key.starts_with('F'))if(auto n=Number(std::string_view(key).substr(1),24);n && *n)return 111+*n;
 for(auto prefix:{"NUMPAD","NUM"})if(key.starts_with(prefix)){
  auto suffix=std::string_view(key).substr(std::string_view(prefix).size());if(suffix.size()==1 && suffix[0]>='0' && suffix[0]<='9')return 96u+unsigned(suffix[0]-'0');
 }return {};
}
std::string MapToken(std::string_view token){
 auto s=Upper(token);
 // AutoClick checks NUM before NUMPAD and would misread NUMPAD1 as NUM0.
 // Always emit its unambiguous NUMn form from the editor.
 if(s.starts_with("NUMPAD"))s="NUM"+s.substr(6);return s;
}
std::optional<std::string> NormalizeText(SettingField const& f,std::string_view text){
 if(f.type==SettingType::Text&&f.external&&f.textFormat==TextFormat::Public){if(PlainText(text,f.minBytes,f.maxBytes,f.identifier))return std::string(text);return {};}
 if(f.type!=SettingType::Text || f.external || f.textFormat==TextFormat::None || text.size()>SettingInputLimit(f))return {};
 for(unsigned char c:text)if(c<32 || c>=127)return {};
 text=Trim(text);
 if(f.textFormat==TextFormat::Delay){
  auto dash=text.find('-');auto first=Number(text.substr(0,dash),5000,true);if(!first)return {};
  if(dash==std::string_view::npos)return std::to_string(*first);
  auto last=Number(text.substr(dash+1),5000,true);if(!last)return {};
  return std::to_string(std::min(*first,*last))+"-"+std::to_string(std::max(*first,*last));
 }
 auto upper=Upper(text);
 if(f.textFormat==TextFormat::SlotMap && (text.empty() || upper=="DEFAULT" || upper=="AUTO"))return std::string("default");
 if(f.textFormat==TextFormat::KeyList && (text.empty() || upper=="NONE" || upper=="OFF" || text=="0"))return std::string("none");
 std::string result;std::array<bool,256> used{};
 std::size_t start{};
 while(start<text.size()){
  auto end=text.find_first_of(",;",start);if(end==std::string_view::npos)end=text.size();
  auto part=Trim(text.substr(start,end-start));if(part.empty())return {};
  if(f.textFormat==TextFormat::SlotMap){
   auto split=part.find_first_of("=:->");if(split==std::string_view::npos)return {};
   auto left=Trim(part.substr(0,split));auto rest=part.substr(split);
   if(rest.starts_with("->"))rest.remove_prefix(2);else rest.remove_prefix(1);
   auto right=Trim(rest);auto key=MapKey(left);
   auto slot=[](std::string_view s)->unsigned{return s.size()==1 && s[0]>='1' && s[0]<='9'?unsigned(s[0]-'0'):0;};
   auto n=slot(right);std::string token=MapToken(left);
   if(!key || !n){n=slot(left);key=MapKey(right);token=MapToken(right);}
   if(!key || !n || used[*key])return {};used[*key]=true;
   if(!result.empty())result+=',';result+=token+"="+std::to_string(n);
  }else{
   auto key=MapKey(part);if(!key || used[*key])return {};used[*key]=true;
   if(!result.empty())result+=',';result+=MapToken(part);
  }
  if(end==text.size())break;start=end+1;if(start==text.size())return {};
 }
 if(result.empty() || result.size()>SettingInputLimit(f))return {};return result;
}
}
std::size_t SettingInputLimit(SettingField const& f){
 if(f.type==SettingType::Text&&f.external&&f.textFormat==TextFormat::Public)return f.maxBytes;
 if(f.type==SettingType::Slider)return 32;
 if(f.type!=SettingType::Text)return 11;
 switch(f.textFormat){case TextFormat::Delay:return 32;case TextFormat::SlotMap:return 255;case TextFormat::KeyList:return 127;default:return 0;}
}
std::string SettingInputError(SettingField const& f){
 if(f.type==SettingType::Text&&f.external)return "Text does not match the declared format or UTF-8 byte limits.";
 if(f.type==SettingType::Slider)return "Enter a value on the declared slider step within its range.";
 switch(f.textFormat){
 case TextFormat::Delay:return "Enter 0-5000 ms or a range such as 120-150. Avoid leading zeros.";
 case TextFormat::SlotMap:return "Use KEY=1 through KEY=9, separated by commas; no duplicate keys. Or use default.";
 case TextFormat::KeyList:return "Use supported key names separated by commas, or none. No duplicate keys.";
 default:return "Enter a whole number from "+std::to_string(f.minimum)+" to "+std::to_string(f.maximum)+".";
 }
}
bool ValidSettingValue(SettingField const& f){
 if(f.type==SettingType::Binding){
  auto s=std::get_if<std::string>(&f.value);if(!s)return false;
  if(f.legacyNumericStrings&&!s->empty()&&(*s)[0]>='0'&&(*s)[0]<='9'){auto n=Number(*s,255,true);return n&&(!*n||modmenu::binding::Allowed(*n));}
  auto binding=modmenu::binding::ParseInput(*s);return binding&&!(binding->modifiers&~f.bindingModifiers);
 }
 if(f.type==SettingType::Slider)return ValidSlider(f);
 if(f.type==SettingType::Boolean)return std::holds_alternative<bool>(f.value);
 if(f.type==SettingType::Key){
  auto s=std::get_if<std::string>(&f.value);if(!s)return false;
  if(!f.external && f.keyFormat!=KeyFormat::Menu)return NativeKey(f,*s).has_value();
  return std::find(f.options.begin(),f.options.end(),*s)!=f.options.end();
 }
 if(f.type==SettingType::Text){auto s=std::get_if<std::string>(&f.value);return s && NormalizeText(f,*s).has_value();}
 auto n=std::get_if<int>(&f.value);return n && *n>=f.minimum && *n<=f.maximum &&
  (f.type!=SettingType::Choice || (*n>=0 && std::size_t(*n)<f.options.size()));
}
bool SetText(SettingField& f,std::string_view text){auto normalized=NormalizeText(f,text);if(!normalized)return false;f.value=*normalized;return true;}
std::string SettingKeyCaption(SettingField const& f){
 auto s=std::get_if<std::string>(&f.value);if(!s)return {};
 if(f.type==SettingType::Binding){auto b=modmenu::binding::ParseInput(*s);if(f.legacyNumericStrings&&!s->empty()&&s->front()>='0'&&s->front()<='9'){auto n=Number(*s,255,true);if(n)b=modmenu::binding::Binding{*n,0};}if(b)return b->key?modmenu::binding::Token(*b):"Unbound";return *s;}
 if(f.external || f.keyFormat==KeyFormat::Menu)return *s=="NONE"?"Unbound":*s;
 auto code=NativeKey(f,*s);if(!code)return *s;
 if(!*code)return "Unbound";
 if((*code>='0' && *code<='9') || (*code>='A' && *code<='Z'))return std::string(1,char(*code));
 if(*code>=112 && *code<=135)return "F"+std::to_string(*code-111);
 if(*code>=96 && *code<=105)return "NUM"+std::to_string(*code-96);
 for(auto [name,vk]:{std::pair{"SPACE",32u},{"ENTER",13u},{"TAB",9u},{"ESCAPE",27u},{"HOME",36u},{"END",35u},
     {"INSERT",45u},{"DELETE",46u},{"PAGEUP",33u},{"PAGEDOWN",34u},{"BACKSPACE",8u},{"[",0xDBu},{"]",0xDDu},{"-",0xBDu},{"+",0xBBu},
     {"SHIFT",16u},{"CTRL",17u},{"ALT",18u},{"UP",38u},{"DOWN",40u},{"LEFT",37u},{"RIGHT",39u},
     {"LSHIFT",160u},{"RSHIFT",161u},{"LCTRL",162u},{"RCTRL",163u},{"LALT",164u},{"RALT",165u},
     {";",0xBAu},{",",0xBCu},{".",0xBEu},{"/",0xBFu},{"`",0xC0u},{"\\",0xDCu},{"'",0xDEu}})if(*code==vk)return name;
 return *s;
}
bool SetBinding(SettingField& f,modmenu::binding::Binding b){
 if(f.type!=SettingType::Binding||!modmenu::binding::Valid(b)||(b.modifiers&~f.bindingModifiers))return false;
 f.value=f.legacyNumericStrings&&!b.modifiers&&b.key>=48&&b.key<=57?modmenu::binding::HexToken(b.key):modmenu::binding::Token(b);return true;
}
}
