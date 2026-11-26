// Exercise the production capture path and real OreUI buttons without a game.
#include <Windows.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <map>
#include <string>
#include <iostream>
#include "ui/OreTheme.h"
#include "ui/Fonts.h"
#include "core/Paths.h"
#include "core/ConfigRegistry.h"
#include "ui/SelfConfig.h"

namespace fixture {
struct Item { ImVec2 center; std::string label; bool disabled; };
std::map<std::string, Item> items;
SHORT physicalKey(int) { return 0; }
}
namespace modmenu::ui::ore {
bool observedButton(char const* id, char const* label, Variant variant = Variant::Secondary,
                    ImVec2 size = {0, 0}, bool disabled = false) {
    bool result = button(id, label, variant, size, disabled);
    auto lo = ImGui::GetItemRectMin(), hi = ImGui::GetItemRectMax();
    fixture::items[id] = {{(lo.x + hi.x) * .5f, (lo.y + hi.y) * .5f}, label,
                          disabled || (ImGui::GetCurrentContext()->CurrentItemFlags & ImGuiItemFlags_Disabled)};
    return result;
}
}
#define GetAsyncKeyState fixture::physicalKey
#define button observedButton
#include "../src/ui/LegacySettings.cpp"
#undef button
#undef GetAsyncKeyState

// The harness uses ImGui's built-in font; no Windows font files or GPU are needed.
namespace modmenu::ui::fonts {
void note(std::string_view) {}
ImFont* body() { return ImGui::GetFont(); }
ImFont* bold() { return ImGui::GetFont(); }
ImFont* title() { return ImGui::GetFont(); }
}
namespace modmenu::paths {
std::filesystem::path selfDir() { return std::filesystem::temp_directory_path() / "ModMenuUiTests"; }
std::filesystem::path modsDir() { return selfDir().parent_path(); }
}
#if __has_include("compat/NativeBridge.h")
// I18n gains an optional native language source in the next review increment.
// Keep this UI fixture independent of native game installation and admission.
namespace modmenu::compat {
bool nativeReady() { return false; }
std::string gameLanguage() { return "en_US"; }
}
#endif

int main() {
    using namespace modmenu;
    namespace l = ui::legacy;
    int failures = 0, checks = 0;
    auto check = [&](bool ok, char const* message) {
        ++checks;
        if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    settings().hotkey = 'K';
    settings().hotkeyModifiers = 5;
    l::beginMenuBinding();
    check(l::captureKey(VK_ESCAPE, true) && settings().hotkey == 'K' && settings().hotkeyModifiers == 5,
          "menu Escape preserves the previous nonzero chord");
    check(l::capturing(), "cancel retains release barrier until Escape is released");
    l::captureKey(VK_ESCAPE, false);
    check(!l::capturing(), "Escape release clears the barrier without another capture");
    l::beginMenuBinding();
    l::captureKey(VK_F6, true);
    check(settings().hotkey == VK_F6 && settings().hotkeyModifiers == 0,
          "a new menu binding is still accepted");
    l::captureKey(VK_F6, false);
    settings().setLanguage("en_US");
    reloadLanguage();
    ui::registerSelfConfig();
    auto screen = ConfigRegistry::instance().get("ModMenuBE");
    bool originalId = false;
    for (auto const& category : screen->categories)
        for (auto const& option : category.options)
            originalId |= option.id == "blockGameInput";
    check(originalId, "self configuration retains upstream blockGameInput option id");
    std::map<std::string, std::string> english;
    for (auto lang : {"en_US", "zh_TW"}) {
        settings().setLanguage(lang);
        reloadLanguage();
        for (auto key : {"passive.capture", "passive.key_rejected", "passive.unbind", "passive.unbound",
                         "passive.reset", "passive.loading", "passive.worker_busy", "passive.input_error"}) {
            check(std::string(tr(key)) != key, "binding and passive control messages have a translation");
            if (std::string(lang) == "en_US")
                english[key] = tr(key);
            else
                check(english[key] != tr(key), "Traditional Chinese strings do not fall back to English");
        }
    }
    settings().setLanguage("en_US");
    reloadLanguage();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 800};
    io.DeltaTime = 1.f / 60.f;
    io.Fonts->AddFontDefault();
    io.Fonts->Build();
    ui::ore::applyStyle(1);
    auto frame = [&](ImVec2 mouse = {-100, -100}, bool down = false) {
        fixture::items.clear();
        io.AddMousePosEvent(mouse.x, mouse.y);
        io.AddMouseButtonEvent(0, down);
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize({900, 700});
        ImGui::Begin("Settings test", nullptr, ImGuiWindowFlags_NoSavedSettings);
        l::draw(600);
        ImGui::End();
        ImGui::Render();
    };
    l::draft = {};
    l::draft.valid = true;
    l::loaded = true;
    menu::SettingField f{"/key", "Key", menu::SettingType::Binding, std::string("LALT+K")};
    f.external = true;
    f.bindingModifiers = binding::AllModifiers;
    l::draft.fields = {f};
    l::field = 0;
    l::capture.Begin({});
    l::captureActive = true;
    l::captureKey(VK_ESCAPE, true);
    check(menu::SettingText(l::draft.fields[0]) == "LALT+K", "plugin Escape leaves its draft unchanged");
    l::captureKey(VK_ESCAPE, false);
    frame();
    frame();
    check(fixture::items.contains("##unbind"), "plugin key fields offer an explicit Unbind button");
    if (fixture::items.contains("##unbind")) {
        auto pos = fixture::items.at("##unbind").center;
        frame(pos, false);
        frame(pos, true);
        frame(pos, false);
        check(menu::SettingText(l::draft.fields[0]) == "NONE", "real Unbind click edits only the plugin draft");
        check(settings().hotkey == VK_F6, "plugin Unbind cannot clear ModMenuBE's own key");
    }
    f.type = menu::SettingType::Key;
    f.value = std::string("F6");
    f.options = {"F6", "F7"};
    l::draft.fields = {f};
    frame();
    check(!fixture::items.contains("##unbind") || fixture::items.at("##unbind").disabled,
          "v1 key fields that exclude NONE cannot be unbound");
    check(!menu::SetKey(l::draft.fields[0], VK_ESCAPE) && !menu::SetKey(l::draft.fields[0], VK_BACK),
          "Escape and Backspace do not implicitly clear legacy keys");
    l::close();
    ImGui::DestroyContext();
    std::cout << checks << " settings UI checks / " << failures << " failures\n";
    return failures ? 1 : 0;
}
