// ModMenuBE's own settings screen, registered through the same API other mods use.
#include "ui/SelfConfig.h"

#include <Windows.h>

#include "core/ConfigRegistry.h"
#include "core/I18n.h"
#include "core/Settings.h"

#include <cstring>

namespace modmenu::ui {

namespace {

struct Key {
    int         vk;
    char const* name;
};
constexpr Key kKeys[] = {
    {VK_F1, "F1"},         {VK_F2, "F2"},           {VK_F3, "F3"},       {VK_F4, "F4"},
    {VK_F5, "F5"},         {VK_F6, "F6"},           {VK_F7, "F7"},       {VK_F8, "F8"},
    {VK_F9, "F9"},         {VK_F10, "F10"},         {VK_F11, "F11"},     {VK_F12, "F12"},
    {VK_INSERT, "Insert"}, {VK_HOME, "Home"},       {VK_END, "End"},     {VK_PRIOR, "Page Up"},
    {VK_NEXT, "Page Down"}, {VK_PAUSE, "Pause"},    {VK_SCROLL, "Scroll Lock"}, {VK_OEM_3, "` (~)"},
};
constexpr int kKeyCount = int(sizeof(kKeys) / sizeof(kKeys[0]));

char const* const kLanguages[] = {"auto", "zh_TW", "en_US"};

void addBool(char const* id, char const* labelKey, std::atomic<bool>* target) {
    ConfigOption o;
    o.kind    = OptionKind::Bool;
    o.id      = id;
    o.label   = tr(labelKey);
    o.user    = target;
    o.getBool = [](void* u) -> int { return static_cast<std::atomic<bool>*>(u)->load() ? 1 : 0; };
    o.setBool = [](void* u, int v) { *static_cast<std::atomic<bool>*>(u) = v != 0; };
    ConfigRegistry::instance().addOption("ModMenuBE", std::move(o));
}

} // namespace

void registerSelfConfig() {
    auto& reg = ConfigRegistry::instance();
    reg.beginScreen("ModMenuBE", tr("settings.title"));
    reg.addCategory("ModMenuBE", "general", tr("general"));

    {
        ConfigOption o;
        o.kind  = OptionKind::Enum;
        o.id    = "hotkey";
        o.label = tr("settings.hotkey");
        for (auto const& k : kKeys) o.choices.emplace_back(k.name);
        o.getInt = [](void*) -> int64_t {
            int vk = settings().hotkey;
            for (int i = 0; i < kKeyCount; ++i)
                if (kKeys[i].vk == vk) return i;
            return -1;
        };
        o.setInt = [](void*, int64_t i) {
            if (i >= 0 && i < kKeyCount) settings().hotkey = kKeys[i].vk;
        };
        reg.addOption("ModMenuBE", std::move(o));
    }
    {
        ConfigOption o;
        o.kind    = OptionKind::Enum;
        o.id      = "language";
        o.label   = tr("settings.language");
        o.choices = {tr("settings.language.auto"), "繁體中文", "English"};
        o.getInt  = [](void*) -> int64_t {
            auto l = settings().language();
            for (int i = 0; i < 3; ++i)
                if (l == kLanguages[i]) return i;
            return 0;
        };
        o.setInt = [](void*, int64_t i) {
            if (i < 0 || i > 2) return;
            settings().setLanguage(kLanguages[i]);
            reloadLanguage();
            // Labels are copied at registration; re-register in the new language.
            registerSelfConfig();
        };
        reg.addOption("ModMenuBE", std::move(o));
    }
    {
        ConfigOption o;
        o.kind     = OptionKind::Float;
        o.id       = "uiScale";
        o.label    = tr("settings.ui_scale");
        o.fmin     = 0.5;
        o.fmax     = 2.0;
        o.step     = 0.05;
        o.format   = "%.2fx";
        o.getFloat = [](void*) -> double { return settings().uiScale.load(); };
        o.setFloat = [](void*, double v) { settings().uiScale = v; };
        reg.addOption("ModMenuBE", std::move(o));
    }
    addBool("checkUpdatesOnOpen", "settings.check_updates", &settings().checkUpdatesOnOpen);
    addBool("fullscreen", "settings.fullscreen", &settings().fullscreen);
    addBool("blockGameInput", "settings.block_input", &settings().pauseInputWhileOpen);
    {
        ConfigOption o;
        o.kind      = OptionKind::String;
        o.id        = "curseforgeApiKey";
        o.label     = tr("settings.curseforge_key");
        o.tooltip   = tr("settings.curseforge_key.tip");
        o.maxBytes  = 128;
        o.getString = [](void*, char* out, uint32_t cap) {
            auto k = settings().curseforgeApiKey();
            strncpy_s(out, cap, k.c_str(), _TRUNCATE);
        };
        o.setString = [](void*, char const* v) { settings().setCurseforgeApiKey(v ? v : ""); };
        reg.addOption("ModMenuBE", std::move(o));
    }
    reg.setOnSave("ModMenuBE", [](void*) { settings().save(); }, nullptr);
}

} // namespace modmenu::ui
