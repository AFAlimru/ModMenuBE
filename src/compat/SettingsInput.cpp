#include "ModMenu/Settings.h"
#include "ModMenu/Slider.h"
#include "ModMenu/TextEditor.h"
#include <algorithm>
#include <charconv>
#include <array>
#include <optional>
namespace menu {
namespace {
std::string_view Trim(std::string_view s) {
    while (!s.empty() && s.front() == ' ')
        s.remove_prefix(1);
    while (!s.empty() && s.back() == ' ')
        s.remove_suffix(1);
    return s;
}
std::optional<unsigned> Number(std::string_view text, unsigned limit, bool nativeBase = false) {
    text = Trim(text);
    if (text.empty())
        return {};
    int base = 10;
    if (nativeBase && text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text.remove_prefix(2);
    } else if (nativeBase && text.size() > 1 && text[0] == '0')
        base = 8;
    unsigned n{};
    auto parsed = std::from_chars(text.data(), text.data() + text.size(), n, base);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || n > limit)
        return {};
    return n;
}
} // namespace
std::size_t SettingInputLimit(SettingField const &f) {
    if (f.type == SettingType::Text)
        return f.maxBytes;
    return f.type == SettingType::Slider ? 32 : 11;
}
std::string SettingInputError(SettingField const &f) {
    if (f.type == SettingType::Text)
        return "Text does not match the declared format or UTF-8 byte limits.";
    if (f.type == SettingType::Slider)
        return "Enter a value on the declared slider step within its range.";
    return "Enter a whole number from " + std::to_string(f.minimum) + " to " +
           std::to_string(f.maximum) + ".";
}
bool ValidSettingValue(SettingField const &f) {
    if (f.type == SettingType::Binding) {
        auto s = std::get_if<std::string>(&f.value);
        if (!s)
            return false;
        if (f.legacyNumericStrings && !s->empty() && (*s)[0] >= '0' && (*s)[0] <= '9') {
            auto n = Number(*s, 255, true);
            return n && (!*n || modmenu::binding::Allowed(*n));
        }
        auto binding = modmenu::binding::ParseInput(*s);
        return binding && !(binding->modifiers & ~f.bindingModifiers);
    }
    if (f.type == SettingType::Slider)
        return ValidSlider(f);
    if (f.type == SettingType::Boolean)
        return std::holds_alternative<bool>(f.value);
    if (f.type == SettingType::Key) {
        auto s = std::get_if<std::string>(&f.value);
        if (!s)
            return false;
        return std::find(f.options.begin(), f.options.end(), *s) != f.options.end();
    }
    if (f.type == SettingType::Text) {
        auto s = std::get_if<std::string>(&f.value);
        return s && PlainText(*s, f.minBytes, f.maxBytes, f.identifier);
    }
    auto n = std::get_if<int>(&f.value);
    return n && *n >= f.minimum && *n <= f.maximum &&
           (f.type != SettingType::Choice || (*n >= 0 && std::size_t(*n) < f.options.size()));
}
bool SetText(SettingField &f, std::string_view text) {
    if (f.type != SettingType::Text || !PlainText(text, f.minBytes, f.maxBytes, f.identifier))
        return false;
    f.value = std::string(text);
    return true;
}
std::string SettingKeyCaption(SettingField const &f) {
    auto s = std::get_if<std::string>(&f.value);
    if (!s)
        return {};
    if (f.type == SettingType::Binding) {
        auto b = modmenu::binding::ParseInput(*s);
        if (f.legacyNumericStrings && !s->empty() && s->front() >= '0' && s->front() <= '9') {
            auto n = Number(*s, 255, true);
            if (n)
                b = modmenu::binding::Binding{*n, 0};
        }
        if (b)
            return modmenu::binding::Token(*b);
    }
    return *s;
}

bool SetBinding(SettingField &f, modmenu::binding::Binding b) {
    if (f.type != SettingType::Binding || !modmenu::binding::Valid(b) || (b.modifiers & ~f.bindingModifiers))
        return false;
    f.value = f.legacyNumericStrings && !b.modifiers && b.key >= 48 && b.key <= 57
                  ? modmenu::binding::HexToken(b.key)
                  : modmenu::binding::Token(b);
    return true;
}
} // namespace menu
