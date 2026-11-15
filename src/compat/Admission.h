#pragma once
#include <cstdint>
namespace modmenu::compat {
struct Observation {bool eligible{},foreground{};unsigned scene{};std::uintptr_t identity{};};
class Admission {
 Observation current_{};bool open_{};unsigned origin_{};std::uintptr_t identity_{};
public:
 bool observe(Observation value){
  if(!value.eligible||!value.scene||!value.identity||(open_&&(value.scene!=origin_||value.identity!=identity_)))open_=false;
  current_=value;return value.eligible&&value.foreground&&value.scene&&value.identity;
 }
 void open(){if(current_.eligible&&current_.foreground&&current_.scene&&current_.identity){open_=true;origin_=current_.scene;identity_=current_.identity;}}
 void close(){open_=false;}
 bool retained()const{return open_;}
 bool visible()const{return open_&&current_.eligible&&current_.foreground;}
};
}
