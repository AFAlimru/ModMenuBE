#include "core/ConfigRegistry.h"

namespace modmenu {

ConfigRegistry& ConfigRegistry::instance() {
    static ConfigRegistry registry;
    return registry;
}

ConfigScreen& ConfigRegistry::screenFor(std::string const& mod) {
    auto& slot = screens_[mod];
    if (!slot) {
        slot      = std::make_shared<ConfigScreen>();
        slot->mod = mod;
    } else if (slot.use_count() > 1) {
        // Copy-on-write: the UI may still be holding the old snapshot.
        slot = std::make_shared<ConfigScreen>(*slot);
    }
    return *slot;
}

bool ConfigRegistry::beginScreen(std::string const& mod, std::string const& title) {
    if (mod.empty()) return false;
    std::lock_guard lock(mutex_);
    auto screen   = std::make_shared<ConfigScreen>();
    screen->mod   = mod;
    screen->title = title;
    // Keep an external opener registered earlier.
    if (auto it = screens_.find(mod); it != screens_.end() && it->second) {
        screen->external     = it->second->external;
        screen->externalUser = it->second->externalUser;
    }
    screens_[mod] = std::move(screen);
    return true;
}

bool ConfigRegistry::addCategory(std::string const& mod, std::string const& id, std::string const& title) {
    if (mod.empty() || id.empty()) return false;
    std::lock_guard lock(mutex_);
    auto& screen = screenFor(mod);
    for (auto const& c : screen.categories) {
        if (c.id == id) return false;
    }
    screen.categories.push_back({id, title.empty() ? id : title, {}});
    return true;
}

bool ConfigRegistry::addOption(std::string const& mod, ConfigOption option) {
    if (mod.empty()) return false;
    std::lock_guard lock(mutex_);
    auto& screen = screenFor(mod);
    if (screen.categories.empty()) screen.categories.push_back({"general", "", {}});
    if (option.kind != OptionKind::Header && !option.id.empty()) {
        for (auto const& c : screen.categories) {
            for (auto const& o : c.options) {
                if (o.id == option.id) return false;
            }
        }
    }
    screen.categories.back().options.push_back(std::move(option));
    return true;
}

bool ConfigRegistry::setOnSave(std::string const& mod, ModMenuActionFn fn, void* user) {
    if (mod.empty()) return false;
    std::lock_guard lock(mutex_);
    auto& screen      = screenFor(mod);
    screen.onSave     = fn;
    screen.onSaveUser = user;
    return true;
}

bool ConfigRegistry::setExternal(std::string const& mod, ModMenuActionFn fn, void* user) {
    if (mod.empty() || !fn) return false;
    std::lock_guard lock(mutex_);
    auto& screen        = screenFor(mod);
    screen.external     = fn;
    screen.externalUser = user;
    return true;
}

bool ConfigRegistry::unregister(std::string const& mod) {
    std::lock_guard lock(mutex_);
    return screens_.erase(mod) > 0;
}

std::shared_ptr<ConfigScreen const> ConfigRegistry::get(std::string const& mod) const {
    std::lock_guard lock(mutex_);
    auto it = screens_.find(mod);
    return it == screens_.end() ? nullptr : it->second;
}

bool ConfigRegistry::has(std::string const& mod) const { return get(mod) != nullptr; }

} // namespace modmenu
