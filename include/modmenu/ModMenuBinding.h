// SPDX-License-Identifier: MIT
// ModMenu public keyboard binding contract 2. No Windows headers or runtime dependency.
#pragma once
#ifndef MODMENU_PUBLIC_KEYBOARD_BINDING_V1_H
#define MODMENU_PUBLIC_KEYBOARD_BINDING_V1_H
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
namespace modmenu::binding {
inline constexpr unsigned ContractVersion=2,Ctrl=1,Shift=2,LeftAlt=4,AllModifiers=7,MaxBytes=48;
using Snapshot=std::array<bool,256>;
struct Binding {unsigned key{},modifiers{};bool operator==(Binding const&)const=default;};
inline bool Modifier(unsigned k){return k==16||k==17||(k>=0xA0&&k<=0xA3);}
inline bool Control(unsigned k){return k==17||k==0xA2||k==0xA3;}
inline bool Allowed(unsigned k){
 return k==8||k==9||k==12||k==13||(k>=16&&k<=26)||(k>=28&&k<=57)||
  (k>=65&&k<=93)||k==95||(k>=96&&k<=135)||(k>=144&&k<=150)||
  (k>=160&&k<=183)||(k>=186&&k<=194)||(k>=219&&k<=223)||
  (k>=225&&k<=231)||(k>=233&&k<=254);
}
struct Name {std::string_view text;unsigned key;};
inline constexpr Name Names[]={
 {"BACKSPACE",8},{"TAB",9},{"CLEAR",12},{"ENTER",13},{"SHIFT",16},{"CTRL",17},{"ALT",18},
 {"PAUSE",19},{"CAPSLOCK",20},{"KANA",21},{"IME_ON",22},{"JUNJA",23},{"FINAL",24},{"HANJA",25},{"IME_OFF",26},
 {"CONVERT",28},{"NONCONVERT",29},{"ACCEPT",30},{"MODECHANGE",31},{"SPACE",32},{"PAGEUP",33},{"PAGEDOWN",34},
 {"END",35},{"HOME",36},{"LEFT",37},{"UP",38},{"RIGHT",39},{"DOWN",40},{"SELECT",41},{"PRINT",42},
 {"EXECUTE",43},{"PRINTSCREEN",44},{"INSERT",45},{"DELETE",46},{"HELP",47},{"LWIN",91},{"RWIN",92},{"APPS",93},{"SLEEP",95},
 {"MULTIPLY",106},{"ADD",107},{"SEPARATOR",108},{"SUBTRACT",109},{"DECIMAL",110},{"DIVIDE",111},
 {"NUMLOCK",144},{"SCROLLLOCK",145},{"LSHIFT",160},{"RSHIFT",161},{"LCTRL",162},{"RCTRL",163},{"LALT",164},{"RALT",165},
 {"BROWSER_BACK",166},{"BROWSER_FORWARD",167},{"BROWSER_REFRESH",168},{"BROWSER_STOP",169},{"BROWSER_SEARCH",170},
 {"BROWSER_FAVORITES",171},{"BROWSER_HOME",172},{"VOLUME_MUTE",173},{"VOLUME_DOWN",174},{"VOLUME_UP",175},
 {"MEDIA_NEXT",176},{"MEDIA_PREVIOUS",177},{"MEDIA_STOP",178},{"MEDIA_PLAY_PAUSE",179},{"LAUNCH_MAIL",180},
 {"LAUNCH_MEDIA",181},{"LAUNCH_APP1",182},{"LAUNCH_APP2",183},{"OEM_1",186},{"OEM_PLUS",187},{"OEM_COMMA",188},
 {"OEM_MINUS",189},{"OEM_PERIOD",190},{"OEM_2",191},{"OEM_3",192},{"ABNT_C1",193},{"ABNT_C2",194},
 {"OEM_4",219},{"OEM_5",220},{"OEM_6",221},{"OEM_7",222},{"OEM_8",223},{"OEM_AX",225},{"OEM_102",226},
 {"ICO_HELP",227},{"ICO_00",228},{"PROCESSKEY",229},{"ICO_CLEAR",230},{"PACKET",231},
 {"ATTN",246},{"CRSEL",247},{"EXSEL",248},{"EREOF",249},{"PLAY",250},{"ZOOM",251},{"NONAME",252},{"PA1",253},{"OEM_CLEAR",254}
};
inline std::string HexToken(unsigned key){constexpr char hex[]="0123456789ABCDEF";if(key>255)return {};return std::string("VK_")+hex[key>>4]+hex[key&15];}
inline std::string KeyToken(unsigned key){
 if(!key)return "NONE";if(!Allowed(key))return {};
 if((key>=48&&key<=57)||(key>=65&&key<=90))return std::string(1,char(key));
 if(key>=112&&key<=135)return "F"+std::to_string(key-111);
 if(key>=96&&key<=105)return "NUMPAD"+std::to_string(key-96);
 for(auto const& n:Names)if(n.key==key)return std::string(n.text);return HexToken(key);
}
inline unsigned PrimaryModifier(unsigned k){return Modifier(k)?(Control(k)?Ctrl:Shift):(k==18||k==164?LeftAlt:0u);}
inline bool Valid(Binding b){return b.modifiers<=AllModifiers&&(!b.key?!b.modifiers:Allowed(b.key)&&(!b.modifiers||!Modifier(b.key))&&!(b.modifiers&PrimaryModifier(b.key)));}
inline std::string Token(Binding b){if(!Valid(b))return {};return std::string(b.modifiers&Ctrl?"CTRL+":"")+(b.modifiers&Shift?"SHIFT+":"")+(b.modifiers&LeftAlt?"LALT+":"")+KeyToken(b.key);}
inline std::optional<Binding> Parse(std::string_view text){
 if(text.empty()||text.size()>MaxBytes)return {};
 Binding b;
 if(text.starts_with("CTRL+")){b.modifiers|=Ctrl;text.remove_prefix(5);}
 if(text.starts_with("SHIFT+")){b.modifiers|=Shift;text.remove_prefix(6);}
 if(text.starts_with("LALT+")){b.modifiers|=LeftAlt;text.remove_prefix(5);}
 if(text=="NONE")return b.modifiers?std::nullopt:std::optional{b};
 if(text.size()==1&&((text[0]>='A'&&text[0]<='Z')||(text[0]>='0'&&text[0]<='9')))b.key=unsigned(text[0]);
 else {
  for(auto const& n:Names)if(n.text==text){b.key=n.key;break;}
  if(!b.key)for(unsigned i=1;i<=24;++i)if(text=="F"+std::to_string(i)){b.key=111+i;break;}
  if(!b.key&&text.size()==7&&text.starts_with("NUMPAD")&&text[6]>='0'&&text[6]<='9')b.key=96+unsigned(text[6]-'0');
  if(!b.key&&text.size()==5&&text.starts_with("VK_")){
   auto digit=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:-1;};
   auto a=digit(text[3]),z=digit(text[4]);if(a<0||z<0)return {};b.key=unsigned(a*16+z);
  }
 }
 if(!b.key||!Valid(b))return {};return b;
}
inline unsigned Modifiers(Snapshot const& s){return (s[17]||s[162]||s[163]?Ctrl:0u)|(s[16]||s[160]||s[161]?Shift:0u)|(s[164]?LeftAlt:0u);}
// Read-only compatibility spellings; new captures always use Token().
inline std::optional<Binding> ParseInput(std::string_view text){
 if(text.size()>MaxBytes)return {};std::string value(text);
 for(char& c:value)if(c>='a'&&c<='z')c=char(c-'a'+'A');
 if(value=="LEFT_ALT")value="LALT";if(value=="RIGHT_ALT")value="RALT";if(value=="UNBOUND")value="NONE";
 if(value=="+")value="OEM_PLUS";
 if(value.size()==1)for(auto n:std::array<Name,11>{{{";",186},{"=",187},{",",188},{"-",189},{".",190},{"/",191},{"`",192},{"[",219},{"\\",220},{"]",221},{"'",222}}})if(n.text==value)return Binding{n.key,0};
 if(value.starts_with("0X")&&value.size()==4)value="VK_"+value.substr(2);
 return Parse(value);
}
inline bool Matches(Binding b,Snapshot const& s){
 if(!b.key||!Valid(b)||!s[b.key])return false;auto mask=Modifiers(s);
 mask&=~PrimaryModifier(b.key);
 return mask==b.modifiers;
}
template<class Read> Snapshot Sample(Binding b,Read read){
 Snapshot s{};if(b.key<256)s[b.key]=read(b.key);
 for(unsigned k:{16u,17u,160u,161u,162u,163u,164u})s[k]=read(k);return s;
}
struct State {bool pressed{},held{};};
class Tracker {
 Binding binding_{};bool known_{},ready_{},down_{},latched_{};
public:
 void Reset(){known_=ready_=down_=latched_=false;}
 State Update(Binding b,Snapshot const& s,bool eligible){
  bool down=b.key<256&&s[b.key];
  if(!eligible||!Valid(b)||!b.key){Reset();return {};}
  if(!known_||b!=binding_){binding_=b;known_=true;ready_=!down;down_=down;latched_=false;return {};}
  if(!down)ready_=true;
  bool matches=Matches(b,s),pressed=ready_&&down&&!down_&&matches;
  if(pressed)latched_=true;if(!matches)latched_=false;
  State result{pressed,latched_&&matches};down_=down;return result;
 }
};
class Capture {
 Snapshot blocked_{};unsigned pending_{},modifiers_{},allowedModifiers_{AllModifiers};bool ambiguous_{},hint_{},active_{};
public:
 bool Active()const{return active_;}
 unsigned PendingModifiers()const{return active_?modifiers_:0;}
 bool NeedsPrimary()const{return active_&&(ambiguous_||hint_);}
 void Cancel(){active_=false;pending_=modifiers_=0;ambiguous_=hint_=false;blocked_={};}
 void Begin(Snapshot const& s,unsigned allowedModifiers=AllModifiers){Cancel();active_=true;blocked_=s;allowedModifiers_=allowedModifiers;}
 std::optional<Binding> Event(unsigned k,bool down,Snapshot const& s){
  if(!active_||k>=256)return {};
  modifiers_=Modifiers(s);
  if(k==27&&down){Cancel();return Binding{};}
  if(blocked_[k]){if(!down)blocked_[k]=false;return {};}
  if(!Allowed(k))return {};
  if(Modifier(k)||(k==164&&(allowedModifiers_&LeftAlt))){
   // Generic aliases are also reported alongside side-specific Windows samples.
   if((k==16&&(s[160]||s[161]))||(k==17&&(s[162]||s[163])))return {};
   if(down){if(!pending_)hint_=false;if(pending_&&pending_!=k)ambiguous_=true;else pending_=k;return {};}
   if(pending_==k&&!ambiguous_){Cancel();return Binding{k,0};}
   if(!Modifiers(s)){hint_=ambiguous_;pending_=0;ambiguous_=false;}return {};
  }
  if(k==18&&s[164])return {}; // Generic Alt aliases a sampled Left Alt.
  if(!down)return {};
  for(unsigned i:{16u,17u,160u,161u,162u,163u,164u})if(blocked_[i]&&s[i])return {};
  Binding b{k,Modifiers(s)&~PrimaryModifier(k)};if(!Valid(b)||(b.modifiers&~allowedModifiers_)){hint_=true;return {};}Cancel();return b;
 }
};
} // namespace modmenu::binding
#endif // MODMENU_PUBLIC_KEYBOARD_BINDING_V1_H
