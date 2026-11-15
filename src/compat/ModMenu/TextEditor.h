#pragma once
#include <string>
#include <string_view>
#include <cstddef>
namespace menu {
bool PlainText(std::string_view,std::size_t minimum,std::size_t maximum,bool identifier=false);
class TextEditor {
public:
 std::string value,error;std::size_t caret{},anchor{},limit{4096};
 void Begin(std::string text,std::size_t maximum,bool select=true);
 bool Insert(std::string_view);
 void Move(int direction,bool select=false);void Edge(bool end,bool select=false);
 void SelectAll();void Erase(bool backwards);std::string Selection()const;
 std::string Visible(std::size_t columns)const;
private: void RemoveSelection();
};
}
