#pragma once

#include "modmenu/ModMenuApi.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace modmenu {

enum class OptionKind { Header, Bool, Int, Float, String, Enum, Button };

struct ConfigOption {
    OptionKind  kind = OptionKind::Header;
    std::string id;
    std::string label;
    std::string tooltip;
    int64_t     imin = 0, imax = 0;
    double      fmin = 0, fmax = 0, step = 0;
    std::string format;
    uint32_t    maxBytes = 256;
    std::vector<std::string> choices;
    void* user = nullptr;
    ModMenuGetBoolFn   getBool   = nullptr;
    ModMenuSetBoolFn   setBool   = nullptr;
    ModMenuGetIntFn    getInt    = nullptr;
    ModMenuSetIntFn    setInt    = nullptr;
    ModMenuGetFloatFn  getFloat  = nullptr;
    ModMenuSetFloatFn  setFloat  = nullptr;
    ModMenuGetStringFn getString = nullptr;
    ModMenuSetStringFn setString = nullptr;
    ModMenuActionFn    onClick   = nullptr;
};

struct ConfigCategory {
    std::string id;
    std::string title;
    std::vector<ConfigOption> options;
};

struct ConfigScreen {
    std::string mod;
    std::string title;
    std::vector<ConfigCategory> categories;
    ModMenuActionFn onSave = nullptr;
    void*           onSaveUser = nullptr;
    ModMenuActionFn external = nullptr;
    void*           externalUser = nullptr;
};

// Screens registered through the C API. Screens are copied on read so the
// UI can iterate without holding the lock while calling into other mods.
class ConfigRegistry {
public:
    static ConfigRegistry& instance();

    bool beginScreen(std::string const& mod, std::string const& title);
    bool addCategory(std::string const& mod, std::string const& id, std::string const& title);
    bool addOption(std::string const& mod, ConfigOption option);
    bool setOnSave(std::string const& mod, ModMenuActionFn fn, void* user);
    bool setExternal(std::string const& mod, ModMenuActionFn fn, void* user);
    bool unregister(std::string const& mod);

    std::shared_ptr<ConfigScreen const> get(std::string const& mod) const;
    bool has(std::string const& mod) const;

private:
    ConfigScreen& screenFor(std::string const& mod); // caller holds mutex_
    mutable std::mutex                                  mutex_;
    std::map<std::string, std::shared_ptr<ConfigScreen>> screens_;
};

} // namespace modmenu
