#pragma once
#include <cstdint>
#include <map>
#include <string>
namespace modmenu::workspace {
struct ViewModel {
 std::string mod,page,message,editing,confirmAction,confirmRow;
 std::uint64_t session{},confirmRevision{};
 std::map<std::string,std::string> text,committed,selectedRows;
 std::map<std::string,bool> booleans;
 std::map<std::string,float> scroll;
 void open(std::string name,std::uint64_t token){*this={};mod=std::move(name);session=token;}
 void suspend(){if(!editing.empty()&&committed.count(editing))text[editing]=committed[editing];editing.clear();confirmAction.clear();confirmRow.clear();confirmRevision=0;}
 bool observe(std::uint64_t token){if(token==session&&token)return true;auto name=mod;open(name,token);return false;}
 bool back(){suspend();if(page.empty())return false;page.clear();return true;}
};
}
