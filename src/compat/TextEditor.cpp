#include "ModMenu/TextEditor.h"
#include "ModMenu/Storage.h"
#include <algorithm>
namespace menu {
namespace {
std::size_t Previous(std::string const&s,std::size_t p){if(!p)return 0;--p;while(p&&(static_cast<unsigned char>(s[p])&192)==128)--p;return p;}
std::size_t Next(std::string const&s,std::size_t p){if(p>=s.size())return s.size();++p;while(p<s.size()&&(static_cast<unsigned char>(s[p])&192)==128)++p;return p;}
}
bool PlainText(std::string_view text,std::size_t minimum,std::size_t maximum,bool identifier){
 if(text.size()<minimum||text.size()>maximum||!ValidUtf8(std::string(text)))return false;
 for(unsigned char c:text)if(c<32||c==127)return false;
 if(identifier){if(text.empty()||text=="."||text=="..")return false;for(unsigned char c:text)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;}
 return true;
}
void TextEditor::Begin(std::string text,std::size_t maximum,bool select){value=std::move(text);limit=maximum;caret=value.size();anchor=select?0:caret;error.clear();}
void TextEditor::RemoveSelection(){auto a=std::min(caret,anchor),b=std::max(caret,anchor);value.erase(a,b-a);caret=anchor=a;}
bool TextEditor::Insert(std::string_view text){
 if(!PlainText(text,0,limit)||value.size()-(std::max(caret,anchor)-std::min(caret,anchor))>limit-text.size()){error="Text is invalid or exceeds its UTF-8 byte limit.";return false;}
 RemoveSelection();value.insert(caret,text);caret+=text.size();anchor=caret;error.clear();return true;
}
void TextEditor::Move(int direction,bool select){if(!select&&caret!=anchor)caret=direction<0?std::min(caret,anchor):std::max(caret,anchor);else caret=direction<0?Previous(value,caret):Next(value,caret);if(!select)anchor=caret;}
void TextEditor::Edge(bool end,bool select){caret=end?value.size():0;if(!select)anchor=caret;}
void TextEditor::SelectAll(){anchor=0;caret=value.size();}
void TextEditor::Erase(bool backwards){if(caret!=anchor)RemoveSelection();else{anchor=backwards?Previous(value,caret):Next(value,caret);RemoveSelection();}error.clear();}
std::string TextEditor::Selection()const{return value.substr(std::min(caret,anchor),std::max(caret,anchor)-std::min(caret,anchor));}
std::string TextEditor::Visible(std::size_t columns)const{columns=std::max(std::size_t(1),columns);auto begin=caret;for(std::size_t n=0;n<columns/2&&begin;++n)begin=Previous(value,begin);std::string out;for(auto p=begin,n=std::size_t(0);n<columns&&p<=value.size();++n){if(p==caret)out+='|';if(p==value.size())break;auto next=Next(value,p);out+=value.substr(p,next-p);p=next;}return out;}
}
