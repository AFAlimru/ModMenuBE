#include "modmenu/ModMenuApi.h"

#include "core/ConfigRegistry.h"
#include "core/Log.h"
#include "core/ModRegistry.h"
#include "ui/MenuControl.h"

#include <nlohmann/json.hpp>

#include <cstring>

namespace {

using namespace modmenu;

std::string s(char const* v) { return v ? std::string(v) : std::string(); }

ConfigOption base(OptionKind kind, char const* id, char const* label, char const* tooltip, void* user) {
    ConfigOption o;
    o.kind    = kind;
    o.id      = s(id);
    o.label   = s(label);
    o.tooltip = s(tooltip);
    o.user    = user;
    return o;
}

int beginScreen(char const* mod, char const* title) { return ConfigRegistry::instance().beginScreen(s(mod), s(title)); }

int addCategory(char const* mod, char const* id, char const* title) {
    return ConfigRegistry::instance().addCategory(s(mod), s(id), s(title));
}

int addHeader(char const* mod, char const* text) {
    if (!text) return 0;
    return ConfigRegistry::instance().addOption(s(mod), base(OptionKind::Header, nullptr, text, nullptr, nullptr));
}

int addBool(char const* mod, char const* id, char const* label, char const* tooltip, ModMenuGetBoolFn get,
            ModMenuSetBoolFn set, void* user) {
    if (!id || !label || !get || !set) return 0;
    auto o    = base(OptionKind::Bool, id, label, tooltip, user);
    o.getBool = get;
    o.setBool = set;
    return ConfigRegistry::instance().addOption(s(mod), std::move(o));
}

int addInt(char const* mod, char const* id, char const* label, char const* tooltip, int64_t min, int64_t max,
           ModMenuGetIntFn get, ModMenuSetIntFn set, void* user) {
    if (!id || !label || !get || !set || min > max) return 0;
    auto o   = base(OptionKind::Int, id, label, tooltip, user);
    o.imin   = min;
    o.imax   = max;
    o.getInt = get;
    o.setInt = set;
    return ConfigRegistry::instance().addOption(s(mod), std::move(o));
}

int addFloat(char const* mod, char const* id, char const* label, char const* tooltip, double min, double max,
             double step, char const* format, ModMenuGetFloatFn get, ModMenuSetFloatFn set, void* user) {
    if (!id || !label || !get || !set || !(min <= max)) return 0;
    auto o     = base(OptionKind::Float, id, label, tooltip, user);
    o.fmin     = min;
    o.fmax     = max;
    o.step     = step;
    o.format   = format ? format : "%.2f";
    o.getFloat = get;
    o.setFloat = set;
    return ConfigRegistry::instance().addOption(s(mod), std::move(o));
}

int addString(char const* mod, char const* id, char const* label, char const* tooltip, uint32_t maxBytes,
              ModMenuGetStringFn get, ModMenuSetStringFn set, void* user) {
    if (!id || !label || !get || !set) return 0;
    auto o      = base(OptionKind::String, id, label, tooltip, user);
    o.maxBytes  = maxBytes < 2 ? 256 : (maxBytes > 65536 ? 65536 : maxBytes);
    o.getString = get;
    o.setString = set;
    return ConfigRegistry::instance().addOption(s(mod), std::move(o));
}

int addEnum(char const* mod, char const* id, char const* label, char const* tooltip, char const* choices,
            ModMenuGetIntFn get, ModMenuSetIntFn set, void* user) {
    if (!id || !label || !choices || !get || !set) return 0;
    auto o = base(OptionKind::Enum, id, label, tooltip, user);
    for (char const* p = choices; *p && o.choices.size() < 256; p += strlen(p) + 1) o.choices.emplace_back(p);
    if (o.choices.empty()) return 0;
    o.getInt = get;
    o.setInt = set;
    return ConfigRegistry::instance().addOption(s(mod), std::move(o));
}

int addButton(char const* mod, char const* id, char const* label, char const* tooltip, ModMenuActionFn onClick,
              void* user) {
    if (!id || !label || !onClick) return 0;
    auto o    = base(OptionKind::Button, id, label, tooltip, user);
    o.onClick = onClick;
    return ConfigRegistry::instance().addOption(s(mod), std::move(o));
}

int setOnSave(char const* mod, ModMenuActionFn fn, void* user) {
    return ConfigRegistry::instance().setOnSave(s(mod), fn, user);
}

int setExternalScreen(char const* mod, ModMenuActionFn fn, void* user) {
    return ConfigRegistry::instance().setExternal(s(mod), fn, user);
}

int setMetadata(char const* mod, char const* json) {
    if (!mod || !json) return 0;
    auto parsed = nlohmann::json::parse(json, nullptr, false);
    if (!parsed.is_object()) return 0;
    ModRegistry::instance().setRuntimeMetadata(mod, json);
    ui::requestRescan();
    return 1;
}

int unregister(char const* mod) {
    if (!mod) return 0;
    ConfigRegistry::instance().unregister(mod);
    ModRegistry::instance().clearRuntimeMetadata(mod);
    ui::requestRescan();
    return 1;
}

int openMenu(char const* mod, int openConfig) {
    ui::requestOpen(s(mod), openConfig != 0);
    return 1;
}

int closeMenu() {
    ui::requestClose();
    return 1;
}

int isMenuOpen() { return ui::isOpen() ? 1 : 0; }

ModMenuApi const kApi = {
    MODMENU_API_VERSION, sizeof(ModMenuApi), beginScreen, addCategory, addHeader, addBool, addInt, addFloat,
    addString, addEnum, addButton, setOnSave, setExternalScreen, setMetadata, unregister, openMenu, closeMenu,
    isMenuOpen,
};

} // namespace

extern "C" MODMENU_EXPORT ModMenuApi const* ModMenu_GetApi(uint32_t minVersion) {
    return minVersion <= MODMENU_API_VERSION ? &kApi : nullptr;
}
