#include "ModMenu/Settings.h"
#include "ModMenu/Slider.h"
#include <algorithm>
#include <charconv>
#include <cstdint>
namespace menu {
namespace {
bool Accepts(SettingField f, SettingValue const &value) {
    f.value = value;
    return ValidSettingValue(f);
}
} // namespace
bool ResetSetting(SettingField &f) {
    if (!Accepts(f, f.defaultValue) || f.value == f.defaultValue)
        return false;
    f.value = f.defaultValue;
    return true;
}
bool ResetSettings(SettingsDraft &d) {
    bool changed = false;
    if (d.valid)
        for (auto &f : d.fields)
            changed = ResetSetting(f) || changed;
    return changed;
}
bool SetInteger(SettingField &f, std::string_view text) {
    if (f.type == SettingType::Slider)
        return SetSliderText(f, text);
    if (f.type != SettingType::Integer || text.empty() || text.size() > 11)
        return false;
    int n{};
    auto r = std::from_chars(text.data(), text.data() + text.size(), n);
    if (r.ec != std::errc{} || r.ptr != text.data() + text.size() || !Accepts(f, SettingValue{n}))
        return false;
    f.value = n;
    return true;
}
bool AdjustSetting(SettingField &f, int steps) {
    if (f.type == SettingType::Slider)
        return StepSlider(f, steps);
    auto n = std::get_if<int>(&f.value);
    if (!n || (f.type != SettingType::Integer && f.type != SettingType::Choice))
        return false;
    auto next = std::clamp(std::int64_t(*n) + std::int64_t(steps) * std::max(1, f.step),
                           std::int64_t(f.minimum), std::int64_t(f.maximum));
    if (next == *n)
        return false;
    *n = int(next);
    return true;
}
} // namespace menu
