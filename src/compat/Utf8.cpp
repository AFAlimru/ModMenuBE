// Extracted from ModMenuBedrock Storage.cpp; MIT, see integration/ModMenuBedrock-LICENSE.txt.
#include "ModMenu/Storage.h"
namespace menu {
bool ValidUtf8(std::string const& s){
 for(std::size_t p=0;p<s.size();){auto c=static_cast<unsigned char>(s[p++]);if(c<128)continue;
  unsigned cp{},n{},min{};
  if(c>=0xc2&&c<=0xdf){cp=c&31;n=1;min=0x80;}else if(c>=0xe0&&c<=0xef){cp=c&15;n=2;min=0x800;}else if(c>=0xf0&&c<=0xf4){cp=c&7;n=3;min=0x10000;}else return false;
  if(n>s.size()-p)return false;while(n--){c=static_cast<unsigned char>(s[p++]);if((c&0xc0)!=0x80)return false;cp=(cp<<6)|(c&63);}
  if(cp<min||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))return false;
 }return true;
}
}
