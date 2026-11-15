#pragma once
#include "PassiveSettings.h"
#include <array>
#include <algorithm>
#include <map>
namespace modmenu::compat {
class DraftInput {
    struct Entry { std::array<char,4097> bytes{}; bool invalid{}; };
    std::map<std::string,Entry> entries_;
public:
    char* buffer(menu::SettingField const& field) {
        auto [it,inserted]=entries_.try_emplace(field.path);
        if(inserted) { auto text=menu::SettingText(field); auto n=std::min(text.size(),it->second.bytes.size()-1);std::copy_n(text.data(),n,it->second.bytes.data()); }
        return it->second.bytes.data();
    }
    bool edit(menu::SettingField& field) {
        auto text=buffer(field);bool valid=false;
        if(field.type==menu::SettingType::Slider)valid=menu::SetSliderText(field,text);
        else if(field.type==menu::SettingType::Integer)valid=menu::SetInteger(field,text);
        else valid=menu::SetText(field,text);
        entries_[field.path].invalid=!valid;return valid;
    }
    bool invalid(std::string const& path)const { auto it=entries_.find(path);return it!=entries_.end()&&it->second.invalid; }
    bool valid()const {for(auto const& [_,v]:entries_)if(v.invalid)return false;return true;}
    void reset(std::string const& path){entries_.erase(path);}
    void clear(){entries_.clear();}
};
}
