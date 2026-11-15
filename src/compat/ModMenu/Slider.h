#pragma once
#include "Settings.h"
#include "json.hpp"
#include <map>
#include <optional>
namespace menu {
using NumberTokens=std::map<std::string,std::string>;
std::optional<std::int64_t> ParseFixed(std::string_view,unsigned precision,bool exponent=true);
std::string FormatFixed(std::int64_t,unsigned precision);
NumberTokens ReadNumberTokens(std::string const&);
std::string DumpNumbers(nlohmann::json const&,NumberTokens const&);
bool ValidSlider(SettingField const&);
bool SetSliderText(SettingField&,std::string_view);
bool StepSlider(SettingField&,int steps);
bool PositionSlider(SettingField&,double ratio);
double SliderRatio(SettingField const&);
}
