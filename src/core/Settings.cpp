#include "core/Settings.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Util.h"

#include <nlohmann/json.hpp>

#include <algorithm>

namespace modmenu {

namespace {
std::filesystem::path settingsPath() { return paths::selfDir() / L"config" / L"config.json"; }
} // namespace

Settings& settings() {
    static Settings instance;
    return instance;
}

std::string Settings::language() const {
    std::lock_guard lock(mutex_);
    return language_;
}
void Settings::setLanguage(std::string v) {
    std::lock_guard lock(mutex_);
    language_ = std::move(v);
}
std::string Settings::curseforgeApiKey() const {
    std::lock_guard lock(mutex_);
    return curseforgeApiKey_;
}
void Settings::setCurseforgeApiKey(std::string v) {
    std::lock_guard lock(mutex_);
    curseforgeApiKey_ = std::move(v);
}

void Settings::load() {
    std::string text;
    if (!readFile(settingsPath(), text)) {
        save(); // write defaults so users can find the file
        return;
    }
    auto j = nlohmann::json::parse(text, nullptr, false, true);
    if (!j.is_object()) {
        log::warn("config.json is not valid JSON, using defaults");
        return;
    }
    hotkey             = j.value("hotkey", hotkey.load());
    showLibraries      = j.value("showLibraries", showLibraries.load());
    sortMode           = j.value("sortMode", sortMode.load());
    checkUpdatesOnOpen = j.value("checkUpdatesOnOpen", checkUpdatesOnOpen.load());
    pauseInputWhileOpen = j.value("blockGameInputWhileOpen", pauseInputWhileOpen.load());
    fullscreen         = j.value("fullscreen", fullscreen.load());
    uiScale            = std::clamp(j.value("uiScale", uiScale.load()), 0.5, 3.0);
    buttonLayoutVersion = j.value("buttonLayoutVersion", buttonLayoutVersion.load());
    setLanguage(j.value("language", std::string("auto")));
    setCurseforgeApiKey(j.value("curseforgeApiKey", std::string()));
}

void Settings::save() const {
    nlohmann::ordered_json j;
    j["hotkey"]                  = hotkey.load();
    j["language"]                = language();
    j["showLibraries"]           = showLibraries.load();
    j["sortMode"]                = sortMode.load();
    j["checkUpdatesOnOpen"]      = checkUpdatesOnOpen.load();
    j["blockGameInputWhileOpen"] = pauseInputWhileOpen.load();
    j["fullscreen"]              = fullscreen.load();
    j["uiScale"]                 = uiScale.load();
    j["curseforgeApiKey"]        = curseforgeApiKey();
#ifndef _WIN32
    j["buttonLayoutVersion"]     = buttonLayoutVersion.load();
#endif
    if (!writeFileAtomic(settingsPath(), j.dump(4))) log::warn("could not write config.json");
}

} // namespace modmenu
