#include "core/I18n.h"

#include "core/Settings.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/system_properties.h>
#endif

#ifdef _WIN32
#define MODMENU_AUTO_LANG_EN "Follow Windows"
#define MODMENU_AUTO_LANG_ZH "跟隨 Windows"
#define MODMENU_DISABLED_EN  "Disabled (manifest.json.close)"
#define MODMENU_DISABLED_ZH  "已停用（manifest.json.close）"
#else
#define MODMENU_DISABLED_EN  "Disabled (LeviLauncher mod list)"
#define MODMENU_DISABLED_ZH  "已停用（LeviLauncher 模組清單）"
#define MODMENU_AUTO_LANG_EN "Follow system"
#define MODMENU_AUTO_LANG_ZH "跟隨系統"
#endif

#include <atomic>
#include <map>
#include <string_view>

namespace modmenu {

namespace {

using Table = std::map<std::string_view, char const*>;

Table const kEnUs = {
    {"title", "Mods"},
    {"count", "%d mods, %d loaded"},
    {"search", "Search mods..."},
    {"show_libraries", "Show libraries"},
    {"sort", "Sort"},
    {"sort.name_asc", "Name A-Z"},
    {"sort.name_desc", "Name Z-A"},
    {"sort.loaded", "Loaded first"},
    {"no_mods", "No mods match your search."},
    {"no_selection", "Select a mod to see its details."},
    {"badge.library", "Library"},
    {"badge.client", "Client"},
    {"badge.deprecated", "Deprecated"},
    {"badge.preload", "Preload"},
    {"badge.levilamina", "LeviLamina"},
    {"badge.disabled", "Disabled"},
    {"badge.modpack", "Modpack"},
    {"badge.update", "Update"},
    {"badge.not_loaded", "Not loaded"},
    {"by", "By %s"},
    {"version", "Version %s"},
    {"config", "Config"},
    {"enable", "Enable"},
    {"disable", "Disable"},
    {"open_folder", "Open folder"},
    {"open_mods_folder", "Mods folder"},
    {"refresh", "Refresh"},
    {"check_updates", "Check updates"},
    {"checking_updates", "Checking..."},
    {"close", "Close"},
    {"back", "Back"},
    {"save", "Save"},
    {"revert", "Revert"},
    {"done", "Done"},
    {"cancel", "Cancel"},
    {"got_it", "Got it"},
    {"commands", "Commands"},
    {"commands.tip", "Closes ModMenuBE and types this into the chat. Works in a world."},
    {"commands.not_loaded", "The mod is not loaded."},
    {"commands.tip_partial", "Closes ModMenuBE and types this into the chat for you to finish (the game suggests the rest). Works in a world."},
    {"restart_notice.disabled", "%s is disabled. Restart Minecraft for this to take effect."},
    {"restart_notice.enabled", "%s is enabled. Restart Minecraft to load it."},
    {"settings.button_layout", "Adjust MOD button layout"},
    {"settings.button_layout.tip", "Opens LeviLauncher's menu: choose the HUD editor there to move, resize or fade the MOD button."},
    {"settings.button_reset", "Reset MOD button position"},
    {"settings.button_reset.tip", "Puts the MOD button back at the top centre, next to the game's own buttons."},
    {"confirm", "Confirm"},
    {"links", "Links"},
    {"link.homepage", "Website"},
    {"link.source", "Source"},
    {"link.issues", "Issues"},
    {"link.discord", "Discord"},
    {"link.wiki", "Wiki"},
    {"link.levimc", "LeviMC"},
    {"description", "Description"},
    {"readme.loading", "Loading README..."},
    {"readme.failed", "Could not load the README."},
    {"readme.none", "No README."},
    {"license", "License"},
    {"authors", "Authors"},
    {"contributors", "Contributors"},
    {"details", "Details"},
    {"type", "Type"},
    {"entry", "Entry"},
    {"folder", "Folder"},
    {"state", "State"},
    {"state.loaded", "Loaded"},
    {"state.not_loaded", "Not loaded"},
    {"state.disabled", MODMENU_DISABLED_EN},
    {"restart_required", "Restart Minecraft to apply mod changes."},
    {"self_disable", "ModMenuBE cannot disable itself. Use LeviLauncher's Manage Mods instead."},
    {"confirm_disable_ll", "Disable LeviLamina? Every LeviLamina mod (type native) will stop loading."},
    {"update.available", "Update available: %s"},
    {"update.uptodate", "Up to date"},
    {"update.checking", "Checking for updates..."},
    {"update.newer_game", "%s needs a newer Minecraft"},
    {"update.error", "Update check failed: %s"},
    {"update.none", "No update source"},
    {"update.open", "Open download page"},
    {"updates_count", "%d updates"},
    {"no_config", "This mod has no settings."},
    {"open.tip", "Opens %s's own menu (%s)"},
    {"open.unbound", "%s has no key bound to open its menu. Bind one in the mod first."},
    {"open.unknown_key", "%s's menu key \"%s\" is not a key ModMenuBE can press."},
    {"json_editor", "Config files"},
    {"json_editor_hint", "Edited directly in the mod's JSON file. Most mods read it at startup, so restart after saving."},
    {"json_parse_error", "This file is not valid JSON: %s"},
    {"unsaved", "Unsaved changes"},
    {"saved", "Saved"},
    {"array_items", "%d items (edit the file to change lists)"},
    {"general", "General"},
    {"other", "Other"},
    {"settings.title", "ModMenuBE"},
    {"settings.hotkey", "Open menu key"},
    {"settings.hotkey.press", "Press a key..."},
    {"settings.language", "Language"},
    {"settings.language.auto", MODMENU_AUTO_LANG_EN},
    {"settings.check_updates", "Check updates when opening"},
    {"settings.block_input", "Block game input while open"},
    {"settings.fullscreen", "Full-screen menu"},
    {"settings.ui_scale", "UI scale"},
    {"settings.curseforge_key", "CurseForge API key"},
    {"settings.curseforge_key.tip", "Needed only for mods that use CurseForge as update source. Empty = CURSEFORGE_API_KEY."},
    {"hint", "%s: open/close  ·  Esc: close"},
    {"hint.android", "MOD button or F8: open/close  ·  Back: close"},
    {"leviLamina_missing", "LeviLamina is not loaded: LeviLamina mods are listed from their manifests only."},
};

Table const kZhTw = {
    {"title", "模組"},
    {"count", "%d 個模組，%d 個已載入"},
    {"search", "搜尋模組…"},
    {"show_libraries", "顯示函式庫"},
    {"sort", "排序"},
    {"sort.name_asc", "名稱 A-Z"},
    {"sort.name_desc", "名稱 Z-A"},
    {"sort.loaded", "已載入優先"},
    {"no_mods", "沒有符合搜尋的模組。"},
    {"no_selection", "選擇一個模組來查看詳細資訊。"},
    {"badge.library", "函式庫"},
    {"badge.client", "客戶端"},
    {"badge.deprecated", "已棄用"},
    {"badge.preload", "前置載入"},
    {"badge.levilamina", "LeviLamina"},
    {"badge.disabled", "已停用"},
    {"badge.modpack", "模組包"},
    {"badge.update", "可更新"},
    {"badge.not_loaded", "未載入"},
    {"by", "作者：%s"},
    {"version", "版本 %s"},
    {"config", "設定"},
    {"enable", "啟用"},
    {"disable", "停用"},
    {"open_folder", "開啟資料夾"},
    {"open_mods_folder", "模組資料夾"},
    {"refresh", "重新整理"},
    {"check_updates", "檢查更新"},
    {"checking_updates", "檢查中…"},
    {"close", "關閉"},
    {"back", "返回"},
    {"save", "儲存"},
    {"revert", "還原"},
    {"done", "完成"},
    {"cancel", "取消"},
    {"got_it", "知道了"},
    {"commands", "指令"},
    {"commands.tip", "關閉 ModMenuBE 並幫你在聊天欄輸入這個指令，需在世界中使用。"},
    {"commands.not_loaded", "這個模組沒有載入。"},
    {"commands.tip_partial", "關閉 ModMenuBE 並在聊天欄打好這個指令，剩下的參數由你補完（遊戲會提示），需在世界中使用。"},
    {"restart_notice.disabled", "已停用「%s」。重新啟動 Minecraft 後才會生效。"},
    {"restart_notice.enabled", "已啟用「%s」。重新啟動 Minecraft 後才會載入。"},
    {"settings.button_layout", "調整 MOD 按鈕佈局"},
    {"settings.button_layout.tip", "打開 LeviLauncher 的選單，在「HUD 編輯器」裡拖曳 MOD 按鈕、調整大小和透明度。"},
    {"settings.button_reset", "重設 MOD 按鈕位置"},
    {"settings.button_reset.tip", "把 MOD 按鈕放回畫面上方中間、遊戲原本按鈕的旁邊。"},
    {"confirm", "確定"},
    {"links", "連結"},
    {"link.homepage", "網站"},
    {"link.source", "原始碼"},
    {"link.issues", "問題回報"},
    {"link.discord", "Discord"},
    {"link.wiki", "Wiki"},
    {"link.levimc", "LeviMC"},
    {"description", "說明"},
    {"readme.loading", "正在載入 README…"},
    {"readme.failed", "無法載入 README。"},
    {"readme.none", "沒有 README。"},
    {"license", "授權"},
    {"authors", "作者"},
    {"contributors", "貢獻者"},
    {"details", "詳細資訊"},
    {"type", "類型"},
    {"entry", "進入點"},
    {"folder", "資料夾"},
    {"state", "狀態"},
    {"state.loaded", "已載入"},
    {"state.not_loaded", "未載入"},
    {"state.disabled", MODMENU_DISABLED_ZH},
    {"restart_required", "重新啟動 Minecraft 後才會套用模組變更。"},
    {"self_disable", "ModMenuBE 無法停用自己，請到 LeviLauncher 的「管理模組」停用。"},
    {"confirm_disable_ll", "確定要停用 LeviLamina？所有 LeviLamina 模組（native 類型）都會無法載入。"},
    {"update.available", "有新版本：%s"},
    {"update.uptodate", "已是最新版本"},
    {"update.checking", "正在檢查更新…"},
    {"update.newer_game", "%s 需要更新版的 Minecraft"},
    {"update.error", "檢查更新失敗：%s"},
    {"update.none", "沒有更新來源"},
    {"update.open", "開啟下載頁面"},
    {"updates_count", "%d 個更新"},
    {"no_config", "這個模組沒有設定。"},
    {"open.tip", "開啟 %s 自己的選單（%s）"},
    {"open.unbound", "%s 沒有綁定開啟選單的按鍵，請先在模組裡設定。"},
    {"open.unknown_key", "%s 的選單按鍵「%s」無法由 ModMenuBE 送出。"},
    {"json_editor", "設定檔"},
    {"json_editor_hint", "直接修改模組的 JSON 設定檔。多數模組只在啟動時讀取，儲存後請重新啟動遊戲。"},
    {"json_parse_error", "這個檔案不是有效的 JSON：%s"},
    {"unsaved", "尚未儲存"},
    {"saved", "已儲存"},
    {"array_items", "%d 個項目（清單請直接編輯檔案）"},
    {"general", "一般"},
    {"other", "其他"},
    {"settings.title", "ModMenuBE"},
    {"settings.hotkey", "開啟選單按鍵"},
    {"settings.hotkey.press", "請按下按鍵…"},
    {"settings.language", "語言"},
    {"settings.language.auto", MODMENU_AUTO_LANG_ZH},
    {"settings.check_updates", "開啟時檢查更新"},
    {"settings.block_input", "開啟時阻擋遊戲輸入"},
    {"settings.fullscreen", "全螢幕顯示選單"},
    {"settings.ui_scale", "介面縮放"},
    {"settings.curseforge_key", "CurseForge API 金鑰"},
    {"settings.curseforge_key.tip", "只有以 CurseForge 為更新來源的模組需要。留空則使用 CURSEFORGE_API_KEY 環境變數。"},
    {"hint", "%s：開啟／關閉 · Esc：關閉"},
    {"hint.android", "MOD 按鈕或 F8：開啟／關閉 · 返回鍵：關閉"},
    {"leviLamina_missing", "LeviLamina 未載入：LeviLamina 模組只會顯示 manifest 資訊。"},
};

std::atomic<Table const*> gActive{&kEnUs};
std::atomic<bool>         gZh{false};

} // namespace

void reloadLanguage() {
    std::string lang = settings().language();
    if (lang == "auto" || lang.empty()) {
#ifdef _WIN32
        LANGID id = GetUserDefaultUILanguage();
        lang      = PRIMARYLANGID(id) == LANG_CHINESE ? "zh_TW" : "en_US";
#else
        char value[PROP_VALUE_MAX]{};
        if (__system_property_get("persist.sys.locale", value) <= 0) __system_property_get("ro.product.locale", value);
        lang = std::string(value).rfind("zh", 0) == 0 ? "zh_TW" : "en_US";
#endif
    }
    bool zh = lang.rfind("zh", 0) == 0;
    gZh     = zh;
    gActive = zh ? &kZhTw : &kEnUs;
}

std::string activeLanguage() { return gZh ? "zh_TW" : "en_US"; }

char const* tr(char const* key) {
    Table const* t = gActive;
    if (auto it = t->find(key); it != t->end()) return it->second;
    if (auto it = kEnUs.find(key); it != kEnUs.end()) return it->second;
    return key;
}

std::vector<std::string> allStrings() {
    std::vector<std::string> out;
    for (auto const* t : {&kEnUs, &kZhTw}) {
        for (auto const& [_, v] : *t) out.emplace_back(v);
    }
    return out;
}

} // namespace modmenu
