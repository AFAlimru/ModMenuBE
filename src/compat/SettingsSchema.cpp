#include "ModMenu/Settings.h"
#include "ModMenu/Slider.h"
#include <algorithm>
#include <charconv>
#include <cstdint>
namespace menu {
namespace {
std::vector<std::string> Keys(bool preview){
 std::vector<std::string> r;
 if(preview)r={"NONE","LALT","LEFT_ALT","RALT","RIGHT_ALT","LCTRL","RCTRL","LSHIFT","RSHIFT","SPACE"};
 else r={"NONE","INSERT","DELETE","HOME","END","PAGEUP","PAGEDOWN","PAUSE"};
 for(int n=1;n<=24;++n)r.push_back("F"+std::to_string(n));
 for(char c='A';c<='Z';++c)r.emplace_back(1,c);for(char c='0';c<='9';++c)r.emplace_back(1,c);return r;
}
bool Accepts(SettingField f,SettingValue const& value){f.value=value;return ValidSettingValue(f);}
}
SettingsDefinition SettingsSchema(std::string const& id){
 SettingsDefinition a;
 auto boolean=[&](char const* path,char const* name,bool value){a.fields.push_back({path,name,SettingType::Boolean,value});};
 auto integer=[&](char const* path,char const* name,int value,int lo,int hi){a.fields.push_back({path,name,SettingType::Integer,value,lo,hi});};
 auto text=[&](char const* path,char const* name,char const* value,TextFormat format){a.fields.push_back({path,name,SettingType::Text,std::string(value)});a.fields.back().textFormat=format;};
 auto key=[&](char const* path,char const* name,char const* value,KeyFormat format){a.fields.push_back({path,name,SettingType::Key,std::string(value)});a.fields.back().keyFormat=format;};
 if(id=="AutoRestock") {boolean("/enabled","Enabled",true);integer("/preRestockThreshold","Restock below",6,1,64);}
 else if(id=="ContainerPreview") {a.limit=4096;boolean("/enabled","Enabled",true);a.fields.push_back({"/holdKey","Hold preview key",SettingType::Key,std::string("LALT"),0,0,Keys(true)});}
 else if(id=="FakeSneak") {boolean("/enabled","Enabled",false);a.fields.push_back({"/toggleKey","Toggle key",SettingType::Key,std::string("F5"),0,0,Keys(false)});boolean("/showNotifications","Notifications",true);}
 else if(id=="MouseTweaksBedrock") {
  a.file="MouseTweaksBedrock.json";a.limit=65536;
  boolean("/enabled","Enabled",true);boolean("/debugLogging","Debug logging",false);boolean("/rmbTweak","Right-click drag",true);
  boolean("/lmbTweakWithItem","Collect matching stacks",true);boolean("/lmbTweakWithoutItem","Shift-transfer drag",true);boolean("/wheelTweak","Mouse wheel",true);
  a.fields.push_back({"/wheelSearchOrder","Search order",SettingType::Choice,1,0,1,{"First to last","Last to first"}});
  a.fields.push_back({"/wheelScrollDirection","Scroll direction",SettingType::Choice,0,0,3,{"Normal","Inverted","Position-aware","Position-aware inverted"}});
  a.fields.push_back({"/scrollItemScaling","Scroll scaling",SettingType::Choice,0,0,1,{"Proportional","Always one"}});
 }else if(id=="AutoClick") {
  a.file="AutoClick.json";a.limit=65536;integer("/cps","Clicks per second",17,1,100);
  integer("/requiredSlot","Required hotbar slot",1,1,9);boolean("/requireLmbHold","Require left button held",true);
  integer("/intervalMs","Fixed click interval",0,0,10000);boolean("/stabilityMode","Stable cadence",true);
  boolean("/disableWhenCursorVisible","Pause when cursor is visible",true);boolean("/wheelInvert","Invert hotbar wheel",false);
  a.fields.push_back({"/uiProtectionMode","Extra cursor protection",SettingType::Choice,1,0,2,{"No movement check","No movement check (legacy)","Pause while cursor moves"}});
  text("/keySlotMap","Hotbar key mapping","1=1,2=2,3=3,4=4,5=5,6=6,7=7,8=8,9=9",TextFormat::SlotMap);
  text("/uiBlockKeys","UI pause keys","none",TextFormat::KeyList);
  key("/keys/toggle","Toggle key","F8",KeyFormat::AutoClick);
 }else if(id=="AutoJumpReset") {
  a.file="AutoJumpReset.json";a.limit=65536;integer("/delayStepMs","Delay step",1,1,250);
  boolean("/overlay/enabled","Reminder overlay",false);integer("/overlay/durationMs","Reminder duration",1400,0,10000);
  integer("/overlay/alpha","Reminder opacity",230,0,255);
  text("/delayMs","Jump delay","134",TextFormat::Delay);boolean("/autoUnhookAfterScan","Unhook after scan",true);
  key("/keys/toggle","Toggle key","J",KeyFormat::AutoJumpReset);
  key("/keys/debug","Debug key","F7",KeyFormat::AutoJumpReset);
  key("/keys/scan","Scan key","F8",KeyFormat::AutoJumpReset);
  key("/keys/hooks","Hooks key","F9",KeyFormat::AutoJumpReset);
  key("/keys/reloadConfig","Reload settings key","F5",KeyFormat::AutoJumpReset);
  key("/keys/bind","Bind target key","F4",KeyFormat::AutoJumpReset);
  key("/keys/jump","Jump key","SPACE",KeyFormat::AutoJumpReset);
  key("/keys/delayDown","Decrease delay key","[",KeyFormat::AutoJumpReset);
  key("/keys/delayUp","Increase delay key","]",KeyFormat::AutoJumpReset);
  key("/keys/stepDown","Decrease step key","-",KeyFormat::AutoJumpReset);
  key("/keys/stepUp","Increase step key","+",KeyFormat::AutoJumpReset);
 }
 for(auto& f:a.fields){
  if(f.path=="/enabled")f.description="Enable this plugin's configured feature; this does not load or unload its DLL.";
  else if(f.path=="/preRestockThreshold")f.description="Restock strictly below this count: 17 means 16 or fewer. Main hand, main inventory only.";
  else if(f.path=="/holdKey"){f.category="Controls";f.description="Hold this key while aiming at a supported container to preview its contents.";}
  else if(f.path=="/toggleKey"){f.category="Controls";f.description="Press to toggle edge protection. Escape or Backspace unbinds the key.";}
  else if(f.path=="/showNotifications")f.description="Show the plugin's toggle notifications.";
  else if(f.path=="/debugLogging"){f.category="Diagnostics";f.description="Write extra diagnostic logs. Leave OFF for normal play.";}
  else if(f.path=="/rmbTweak"){f.category="Dragging";f.description="Right-drag places one item in each visited slot, including revisits.";}
  else if(f.path=="/lmbTweakWithItem"){f.category="Dragging";f.description="Left-drag with an item collects matching stacks; Shift transfers matching stacks.";}
  else if(f.path=="/lmbTweakWithoutItem"){f.category="Dragging";f.description="Shift + held left-drag quickly transfers visited stacks to the other inventory.";}
  else if(f.path=="/wheelTweak"){f.category="Mouse wheel";f.description="Use the mouse wheel over a slot to transfer individual items.";}
  else if(f.path=="/wheelSearchOrder"){f.category="Mouse wheel";f.description="Order used to search slots in the other inventory.";}
  else if(f.path=="/wheelScrollDirection"){f.category="Mouse wheel";f.description="Choose normal, reversed or slot-position-aware transfer direction.";}
  else if(f.path=="/scrollItemScaling"){f.category="Mouse wheel";f.description="Transfer proportionally to scrolling, or exactly one item per wheel action.";}
  else if(f.path=="/cps"){f.category="Timing";f.unit="CPS";f.description="Requested click rate. The plugin's existing safety and UI gates still apply.";}
  else if(f.path=="/requiredSlot"){f.category="Controls";f.description="Required hotbar slot, numbered 1 to 9.";}
  else if(f.path=="/requireLmbHold"){f.category="Controls";f.description="Require the physical left mouse button to be held; does not enable the session.";}
  else if(f.path=="/delayStepMs"){f.category="Timing";f.unit="ms";f.description="Adjustment step for the plugin's delay controls, not the jump delay itself.";}
  else if(f.path=="/overlay/enabled"){f.category="Reminder";f.description="Enable the reminder overlay.";}
  else if(f.path=="/overlay/durationMs"){f.category="Reminder";f.unit="ms";f.step=50;f.description="How long a reminder remains visible. Type an exact value or adjust in 50 ms steps.";}
  else if(f.path=="/overlay/alpha"){f.category="Reminder";f.description="Reminder opacity from 0 (transparent) to 255 (opaque).";}
  else if(f.path=="/intervalMs"){f.category="Timing";f.unit="ms";f.description="0 uses CPS. A positive interval overrides CPS and the 16 CPS stable-cadence cap.";}
  else if(f.path=="/stabilityMode"){f.category="Timing";f.description="Use stable cadence, capped at 16 CPS only when fixed interval is 0. Native UI checks remain active.";}
  else if(f.path=="/disableWhenCursorVisible"){f.category="Controls";f.description="Pause clicks when the Windows cursor is visible. Verified native UI checks always remain active.";}
  else if(f.path=="/wheelInvert"){f.category="Controls";f.description="Reverse the wheel direction used to track the selected hotbar slot.";}
  else if(f.path=="/uiProtectionMode"){f.category="Controls";f.description="Extra cursor heuristic. Visible-cursor pause is controlled separately; native UI checks cannot be disabled.";}
  else if(f.path=="/keySlotMap"){f.category="Controls";f.description="Map keys to hotbar slots, e.g. R=1,E=2,1=7. Use default for standard number keys. No duplicate keys.";}
  else if(f.path=="/uiBlockKeys"){f.category="Controls";f.description="Keys that toggle an extra UI pause, e.g. MOUSE5,TAB; none disables this extra pause, not native checks.";}
  else if(f.path=="/delayMs"){f.category="Timing";f.unit="ms";f.description="Actual jump delay: 134 for fixed delay, or 120-150 for a random range. Bounds: 0-5000 ms. Avoid leading zeros.";}
  else if(f.path=="/autoUnhookAfterScan"){f.category="Diagnostics";f.description="Retain the plugin's automatic unhook behavior after scanning. Advanced setting; normally leave ON.";}
  else if(f.path.starts_with("/keys/")){
   bool advanced=f.path=="/keys/debug" || f.path=="/keys/scan" || f.path=="/keys/hooks" || f.path=="/keys/bind";
   f.category=advanced?"Advanced keys":"Controls";
   f.description=advanced?"Existing diagnostic or target-binding shortcut; changing it does not execute the action. Escape unbinds the key.":"Single keyboard key, including digits and punctuation. Saved in the plugin's native VK format. Escape unbinds the key.";
  }
 }
 return a;
}
bool ResetSetting(SettingField& f){if(!Accepts(f,f.defaultValue) || f.value==f.defaultValue)return false;f.value=f.defaultValue;return true;}
bool ResetSettings(SettingsDraft& d){bool changed=false;if(d.valid)for(auto& f:d.fields)changed=ResetSetting(f)||changed;return changed;}
bool SetInteger(SettingField& f,std::string_view text){
 if(f.type==SettingType::Slider)return SetSliderText(f,text);
 if(f.type!=SettingType::Integer || text.empty() || text.size()>11)return false;
 int n{};auto r=std::from_chars(text.data(),text.data()+text.size(),n);
 if(r.ec!=std::errc{} || r.ptr!=text.data()+text.size() || !Accepts(f,SettingValue{n}))return false;
 f.value=n;return true;
}
bool AdjustSetting(SettingField& f,int steps){
 if(f.type==SettingType::Slider)return StepSlider(f,steps);
 auto n=std::get_if<int>(&f.value);if(!n || (f.type!=SettingType::Integer && f.type!=SettingType::Choice))return false;
 auto next=std::clamp(std::int64_t(*n)+std::int64_t(steps)*std::max(1,f.step),std::int64_t(f.minimum),std::int64_t(f.maximum));
 if(next==*n)return false;*n=int(next);return true;
}
}
