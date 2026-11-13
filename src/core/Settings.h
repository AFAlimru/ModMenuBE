#pragma once

#include <atomic>
#include <mutex>
#include <string>

namespace modmenu {

enum class SortMode : int { NameAsc = 0, NameDesc = 1, LoadedFirst = 2 };

// ModMenuBE's own settings, stored in mods/ModMenuBE/config/config.json.
struct Settings {
    std::atomic<int>    hotkey{0x77}; // VK_F8
    std::atomic<bool>   showLibraries{false};
    std::atomic<int>    sortMode{int(SortMode::NameAsc)};
    std::atomic<bool>   checkUpdatesOnOpen{true};
    std::atomic<bool>   pauseInputWhileOpen{true};
    std::atomic<bool>   fullscreen{false}; // menu fills the whole window instead of a centred panel
    std::atomic<double> uiScale{1.0};
    std::atomic<int>    buttonLayoutVersion{0}; // Android: last default MOD button layout applied

    std::string language() const;          // "auto", "zh_TW", "en_US"
    void        setLanguage(std::string v);
    std::string curseforgeApiKey() const;  // empty = use CURSEFORGE_API_KEY env var
    void        setCurseforgeApiKey(std::string v);

    void load();
    void save() const;

private:
    mutable std::mutex mutex_;
    std::string        language_{"auto"};
    std::string        curseforgeApiKey_;
};

Settings& settings();

} // namespace modmenu
