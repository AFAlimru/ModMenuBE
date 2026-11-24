#include "core/Paths.h"
#include "core/Settings.h"
#include "core/Util.h"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <string>

namespace {
std::filesystem::path testRoot;
int failures = 0;
void check(bool value, char const *message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
void config(std::string const &text) {
    check(modmenu::writeFileAtomic(testRoot / "config/config.json", text), "write isolated fixture");
}
} // namespace

namespace modmenu::paths {
std::filesystem::path selfDir() {
    return testRoot;
}
std::filesystem::path modsDir() {
    return testRoot.parent_path();
}
} // namespace modmenu::paths

int main() {
    namespace fs = std::filesystem;
    testRoot = fs::temp_directory_path() / ("ModMenuSettingsCompat-" + std::to_string(GetCurrentProcessId()));
    fs::create_directories(testRoot / "config");
    struct Cleanup {
        ~Cleanup() {
            std::error_code error;
            fs::remove_all(testRoot, error);
        }
    } cleanup;
    modmenu::Settings settings;
    check(settings.hotkey == 0x77, "upstream default remains F8");
    check(settings.pauseInputWhileOpen, "upstream input protection defaults on");
    settings.hotkey = 'K';
    settings.hotkeyModifiers = 7;
    settings.pauseInputWhileOpen = false;
    settings.save();
    std::string text;
    check(modmenu::readFile(testRoot / "config/config.json", text), "read saved fixture");
    auto json = nlohmann::json::parse(text);
    check(json["hotkey"].is_number_integer(), "hotkey remains an integer for old readers");
    check(json.value("hotkeyModifiers", 0) == 7, "modifiers saved in separate mask");
    check(!json.value("blockGameInputWhileOpen", true), "input blocking setting persists");
    try {
        check(json.value("hotkey", 0) == 'K', "upstream integer reader accepts saved key");
    } catch (...) {
        check(false, "upstream integer reader must not throw");
    }
    settings.hotkey = 0x77;
    settings.hotkeyModifiers = 0;
    settings.load();
    check(settings.hotkey == 'K' && settings.hotkeyModifiers == 7, "integer chord round trip");
    config(R"({"hotkey":117})");
    settings.load();
    check(settings.hotkey == 117 && settings.hotkeyModifiers == 0,
          "old integer config clears stale modifiers");
    config(R"({"hotkey":"CTRL+LALT+K"})");
    settings.load();
    check(settings.hotkey == 'K' && settings.hotkeyModifiers == 5,
          "local string config migrates without loss");
    settings.save();
    modmenu::readFile(testRoot / "config/config.json", text);
    json = nlohmann::json::parse(text);
    check(json["hotkey"].is_number_integer() && json.value("hotkeyModifiers", 0) == 5,
          "migration writes backward-compatible integer and modifier mask");
    for (auto mask : {"8", "-1", "4294967296", "\"CTRL\""}) {
        config(std::string("{\"hotkey\":75,\"hotkeyModifiers\":") + mask + "}");
        settings.load();
        check(settings.hotkey == 'K' && settings.hotkeyModifiers == 0, "invalid modifier mask rejected");
    }
    config(R"({"hotkey":0,"hotkeyModifiers":7})");
    settings.load();
    check(settings.hotkey == 0 && settings.hotkeyModifiers == 0, "unbound key has no modifiers");
    std::cout << "Settings compatibility failures: " << failures << '\n';
    return failures ? 1 : 0;
}
