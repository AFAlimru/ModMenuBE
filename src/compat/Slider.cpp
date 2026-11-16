#include "ModMenu/Slider.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace menu {
namespace {
std::int64_t Scale(unsigned p){std::int64_t n=1;while(p--)n*=10;return n;}
std::string Escape(std::string_view s){std::string r;for(char c:s){if(c=='~')r+="~0";else if(c=='/')r+="~1";else r+=c;}return r;}
struct NumericSax:nlohmann::json_sax<nlohmann::json> {
 struct Level{std::string path,key;bool object{};std::size_t index{};};
 std::vector<Level> stack;NumberTokens numbers;
 std::string Path(){if(stack.empty())return {};auto& l=stack.back();return l.path+"/"+(l.object?Escape(l.key):std::to_string(l.index));}
 void Consume(){if(!stack.empty() && !stack.back().object)++stack.back().index;}
 bool null()override{Consume();return true;}
 bool boolean(bool)override{Consume();return true;}
 bool number_integer(number_integer_t n)override{numbers[Path()]=std::to_string(n);Consume();return true;}
 bool number_unsigned(number_unsigned_t n)override{numbers[Path()]=std::to_string(n);Consume();return true;}
 bool number_float(number_float_t,string_t const& token)override{numbers[Path()]=token;Consume();return true;}
 bool string(string_t&)override{Consume();return true;}
 bool binary(binary_t&)override{return false;}
 bool key(string_t& s)override{stack.back().key=s;return true;}
 bool start_object(std::size_t)override{return Start(true);}
 bool start_array(std::size_t)override{return Start(false);}
 bool Start(bool object){auto path=Path();Consume();if(stack.size()>=33)return false;stack.push_back({std::move(path),{},object});return true;}
 bool end_object()override{stack.pop_back();return true;}
 bool end_array()override{stack.pop_back();return true;}
 bool parse_error(std::size_t,std::string const&,nlohmann::detail::exception const&)override{return false;}
};
void Dump(nlohmann::json const& j,NumberTokens const& numbers,std::string const& path,unsigned depth,std::string& out){
 if(auto it=numbers.find(path);it!=numbers.end()){out+=it->second;return;}
 if(!j.is_structured()){out+=j.dump();return;}
 bool object=j.is_object();out+=object?'{':'[';
 bool first=true;std::size_t index{};
 for(auto it=j.begin();it!=j.end();++it){if(!first)out+=',';first=false;out+='\n';out.append((depth+1)*2,' ');
  auto key=object?it.key():std::to_string(index++);
  if(object){out+=nlohmann::json(key).dump();out+=": ";}
  Dump(it.value(),numbers,path+"/"+Escape(key),depth+1,out);
 }
 if(!first){out+='\n';out.append(depth*2,' ');}out+=object?'}':']';
}
}
std::optional<std::int64_t> ParseFixed(std::string_view text,unsigned precision,bool exponent){
 if(precision>6 || text.empty() || text.size()>65536)return {};
 std::size_t at{};bool negative=text[at]=='-';if(negative && ++at==text.size())return {};
 auto start=at;while(at<text.size() && text[at]>='0' && text[at]<='9')++at;
 if(at==start || (at-start>1 && text[start]=='0'))return {};
 std::string digits(text.substr(start,at-start));int fractional{};
 if(at<text.size() && text[at]=='.'){
  start=++at;while(at<text.size() && text[at]>='0' && text[at]<='9')++at;
  if(at==start)return {};fractional=int(at-start);digits+=text.substr(start,at-start);
 }
 int power{};
 if(at<text.size() && (text[at]=='e' || text[at]=='E')){
  if(!exponent)return {};++at;bool minus=at<text.size() && text[at]=='-';
  if(at<text.size() && (text[at]=='-' || text[at]=='+'))++at;start=at;
  while(at<text.size() && text[at]>='0' && text[at]<='9'){if(power>65536)return {};power=power*10+text[at++]-'0';}
  if(at==start)return {};if(minus)power=-power;
 }
 if(at!=text.size())return {};
 auto nonzero=digits.find_first_not_of('0');if(nonzero==std::string::npos)return 0;
 digits.erase(0,nonzero);int shift=power-fractional+int(precision);
 while(shift<0 && !digits.empty() && digits.back()=='0'){digits.pop_back();++shift;}
 if(shift<0 || shift>16 || digits.size()+std::size_t(shift)>16)return {};
 auto limit=(negative?2147483648LL:2147483647LL)*Scale(precision);std::int64_t n{};
 for(char c:digits){if(n>(limit-(c-'0'))/10)return {};n=n*10+c-'0';}
 while(shift--){if(n>limit/10)return {};n*=10;}
 if(n>limit)return {};return negative?-n:n;
}
std::string FormatFixed(std::int64_t ticks,unsigned precision){
 if(precision>6)throw std::runtime_error("Invalid slider precision");
 bool negative=ticks<0;auto magnitude=negative?std::uint64_t(-(ticks+1))+1:std::uint64_t(ticks);auto s=std::to_string(magnitude);
 if(precision){if(s.size()<=precision)s.insert(0,precision+1-s.size(),'0');s.insert(s.size()-precision,1,'.');while(s.back()=='0')s.pop_back();if(s.back()=='.')s.pop_back();}
 return negative && magnitude?"-"+s:s;
}
NumberTokens ReadNumberTokens(std::string const& bytes){NumericSax sax;if(!nlohmann::json::sax_parse(bytes,&sax))throw std::runtime_error("Invalid numeric JSON");return std::move(sax.numbers);}
std::string DumpNumbers(nlohmann::json const& j,NumberTokens const& numbers){if(numbers.empty())return j.dump(2);std::string out;Dump(j,numbers,{},0,out);return out;}
bool ValidSlider(SettingField const& field){
 if(field.type!=SettingType::Slider)return false;auto const& s=field.slider;auto n=std::get_if<SliderValue>(&field.value);
 if(!n || s.precision>6 || (s.decimal?s.precision==0:s.precision!=0))return false;
 auto scale=Scale(s.precision);
 if(s.minimum<-2147483648LL*scale || s.maximum>2147483647LL*scale || s.minimum>=s.maximum || s.step<=0 || s.step>2147483647LL*scale)return false;
 auto width=s.maximum-s.minimum;
 return s.step<=width && width%s.step==0 && n->ticks>=s.minimum && n->ticks<=s.maximum && (n->ticks-s.minimum)%s.step==0;
}
bool SetSliderText(SettingField& field,std::string_view text){
 if(field.type!=SettingType::Slider || (!field.slider.decimal && text.find_first_of(".eE")!=text.npos))return false;
 auto n=ParseFixed(text,field.slider.precision,false);if(!n)return false;
 auto copy=field;copy.value=SliderValue{*n};if(!ValidSlider(copy))return false;field.value=copy.value;return true;
}
bool StepSlider(SettingField& field,int steps){
 if(!ValidSlider(field) || !steps)return false;auto& n=std::get<SliderValue>(field.value).ticks;auto const& s=field.slider;
 // Work in step indices so int32 steps times a large scaled step cannot overflow.
 auto index=(n-s.minimum)/s.step,count=(s.maximum-s.minimum)/s.step;
 auto next=s.minimum+std::clamp(index+std::int64_t(steps),std::int64_t(0),count)*s.step;
 if(n==next)return false;n=next;return true;
}
bool PositionSlider(SettingField& field,double ratio){
 if(!ValidSlider(field) || !std::isfinite(ratio))return false;auto const& s=field.slider;
 auto count=(s.maximum-s.minimum)/s.step;auto index=std::int64_t(std::floor(std::clamp(ratio,0.,1.)*double(count)+.5));
 auto next=s.minimum+std::clamp(index,std::int64_t(0),count)*s.step;auto& n=std::get<SliderValue>(field.value).ticks;
 if(n==next)return false;n=next;return true;
}
double SliderRatio(SettingField const& field){if(!ValidSlider(field))return 0.;auto const& s=field.slider;return double(std::get<SliderValue>(field.value).ticks-s.minimum)/double(s.maximum-s.minimum);}
}
