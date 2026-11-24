#ifdef _WIN32
#include "LegacySettings.h"
#include "compat/SettingsService.h"
#include "compat/DraftInput.h"
#include "core/Settings.h"
#include "core/I18n.h"
#include "ui/OreTheme.h"
#include "ui/Fonts.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <array>
#include <climits>
#include <atomic>
namespace modmenu::ui::legacy {
namespace {
menu::SettingsDraft draft;
std::filesystem::path root;
std::uint64_t ticket{};
int field = -1, category{};
binding::Capture capture;
binding::Snapshot keys{};
compat::DraftInput inputs;
bool selfBinding{};
bool loaded{};
std::atomic<bool> captureActive{};
bool releaseBarrier{};
std::string caption(menu::SettingTranslation const &t, std::string const &fallback) {
    auto lang = activeLanguage();
    if (lang == "zh_CN" && !t.cn.empty())
        return t.cn;
    if (lang == "zh_TW" && !t.tw.empty())
        return t.tw;
    return t.en.empty() ? fallback : t.en;
}
void label(std::string const &s) {
    fonts::note(s);
    ore::textWrapped(s.c_str());
}
} // namespace
bool available(ModInfo const &m) {
    return m.loaded && m.enabled && m.legacySettings && !(m.adapter && m.adapter->configDisabled);
}
void suspend() {
    capture.Cancel();
    captureActive = false;
    field = -1;
    keys = {};
    releaseBarrier = false;
    selfBinding = false;
    if (ImGui::GetCurrentContext())
        ImGui::ClearActiveID();
}
void close() {
    suspend();
    if (ticket)
        compat::settingsService().discard(ticket);
    ticket = 0;
    draft = {};
    loaded = false;
    inputs.clear();
}
void open(ModInfo const &m) {
    close();
    root = m.folder.parent_path();
    ticket = compat::settingsService().load(root, m.folder.filename().string());
    draft.id = m.folder.filename().string();
    draft.message = ticket ? "Loading settings..." : "Settings worker is busy; reopen this page.";
    category = 0;
}
bool capturing() {
    return captureActive;
}
void beginMenuBinding() {
    suspend();
    selfBinding = true;
    keys = {};
    for (unsigned k = 8; k < 255; ++k)
        keys[k] = (GetAsyncKeyState(int(k)) & 0x8000) != 0;
    capture.Begin(keys);
    captureActive = true;
}

bool captureKey(unsigned key, bool down) {
    if (key >= 256)
        return false;
    if (!capture.Active() && !releaseBarrier)
        return false;
    keys[key] = down;
    keys[16] = keys[160] || keys[161];
    keys[17] = keys[162] || keys[163];
    keys[18] = keys[164] || keys[165];
    if (releaseBarrier) {
        bool held = false;
        for (bool k : keys)
            held |= k;
        if (!held) {
            releaseBarrier = false;
            captureActive = false;
        }
        return true;
    }
    if (auto value = capture.Event(key, down, keys)) {
        if (selfBinding) {
            settings().hotkey = int(value->key);
            settings().hotkeyModifiers = value->modifiers;
            selfBinding = false;
        } else if (field >= 0 && std::size_t(field) < draft.fields.size()) {
            auto &f = draft.fields[field];
            bool accepted = f.type == menu::SettingType::Binding
                                ? menu::SetBinding(f, *value)
                                : menu::SetKey(f, value->key ? value->key : 27);
            if (!accepted)
                draft.message = "This legacy field does not accept that key; its value was preserved.";
        }
        field = -1;
        releaseBarrier = true;
    }
    return true;
}
void draw(float height) {
    if (ticket) {
        menu::SettingsDraft result;
        if (compat::settingsService().take(ticket, result)) {
            draft = std::move(result);
            ticket = 0;
            loaded = draft.valid;
            inputs.clear();
        }
    }
    label(draft.message);
    if (!loaded)
        return;
    std::vector<std::string> categories;
    for (auto const &f : draft.fields)
        if (std::find(categories.begin(), categories.end(), f.category) == categories.end())
            categories.push_back(f.category);
    if (categories.size() > 1) {
        for (std::size_t i = 0; i < categories.size(); ++i) {
            if (i)
                ImGui::SameLine();
            if (ore::button(std::to_string(i).c_str(),
                            caption(std::find_if(draft.fields.begin(), draft.fields.end(),
                                                 [&](auto const &f) { return f.category == categories[i]; })
                                        ->categoryText,
                                    categories[i])
                                .c_str(),
                            int(i) == category ? ore::Variant::Primary : ore::Variant::Dark))
                category = int(i);
        }
        ImGui::Separator();
    }
    ImGui::BeginChild("passive-fields", {0, std::max(0.f, height - ore::s(70))});
    for (std::size_t i = 0; i < draft.fields.size(); ++i) {
        auto &f = draft.fields[i];
        if (!categories.empty() &&
            f.category != categories[std::clamp(category, 0, int(categories.size()) - 1)])
            continue;
        ImGui::PushID(int(i));
        label(caption(f.labelText, f.label));
        auto description = caption(f.descriptionText, f.description);
        if (!description.empty()) {
            fonts::note(description);
            ore::textWrapped(description.c_str(), ore::color::TextDim);
        }
        ImGui::BeginDisabled(ticket != 0);
        if (f.type == menu::SettingType::Boolean) {
            auto v = std::get<bool>(f.value);
            if (ore::toggle("##v", &v))
                f.value = v;
        } else if (f.type == menu::SettingType::Choice) {
            auto v = std::get<int>(f.value);
            if (ImGui::BeginCombo("##v",
                                  caption(f.optionText.size() > std::size_t(v) ? f.optionText[v]
                                                                               : menu::SettingTranslation{},
                                          f.options[v])
                                      .c_str())) {
                for (std::size_t n = 0; n < f.options.size(); ++n) {
                    auto text =
                        n < f.optionText.size() ? caption(f.optionText[n], f.options[n]) : f.options[n];
                    fonts::note(text);
                    if (ImGui::Selectable(text.c_str(), int(n) == v))
                        f.value = int(n);
                }
                ImGui::EndCombo();
            }
        } else if (f.type == menu::SettingType::Key || f.type == menu::SettingType::Binding) {
            auto text = field == int(i) && capture.Active() ? "Press keys (Esc = Unbound)"
                                                            : menu::SettingKeyCaption(f);
            if (ore::button("##binding", text.c_str())) {
                field = int(i);
                keys = {};
                for (unsigned k = 8; k < 255; ++k)
                    keys[k] = (GetAsyncKeyState(int(k)) & 0x8000) != 0;
                capture.Begin(keys, f.type == menu::SettingType::Binding ? f.bindingModifiers : 0);
                captureActive = true;
            }
        } else {
            if (f.type == menu::SettingType::Slider) {
                double ratio = menu::SliderRatio(f), lo = 0, hi = 1;
                if (ImGui::SliderScalar("##slider", ImGuiDataType_Double, &ratio, &lo, &hi, "")) {
                    menu::PositionSlider(f, ratio);
                    inputs.reset(f.path);
                }
            }
            auto *buf = inputs.buffer(f);
            fonts::note(buf);
            if (ImGui::InputText("##v", buf, 4097))
                inputs.edit(f);
            if (inputs.invalid(f.path)) {
                auto error = menu::SettingInputError(f);
                fonts::note(error);
                ore::textWrapped(error.c_str(), ore::color::Red);
            }
        }
        if (ore::button("##reset", "Reset", ore::Variant::Dark)) {
            menu::ResetSetting(f);
            inputs.reset(f.path);
        }
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();
}
void footer(float width) {
    const auto min = ImGui::GetCursorScreenPos();
    const float height = ore::s(40);
    const float end = min.x + std::max(0.f, width);
    ImGui::PushClipRect(min, {end, min.y + height}, true);
    ImGui::BeginDisabled(!loaded || ticket || !inputs.valid());
    if (ore::button("##apply-passive", tr("passive.apply"), ore::Variant::Primary, {0, height})) {
        suspend();
        ticket = compat::settingsService().apply(root, draft);
        if (!ticket)
            draft.message = "Settings worker is busy; try again.";
    }
    ImGui::EndDisabled();
    const float textX = ImGui::GetItemRectMax().x + ore::s(12);
    const char *text = tr("passive.discard_hint");
    fonts::note(text);
    if (textX < end) {
        ImGui::GetWindowDrawList()->PushClipRect({textX, min.y}, {end, min.y + height}, true);
        ImGui::GetWindowDrawList()->AddText({textX, min.y + (height - ImGui::GetFontSize()) * .5f},
                                            ore::color::TextDim, text);
        ImGui::GetWindowDrawList()->PopClipRect();
    }
    ImGui::PopClipRect();
}
} // namespace modmenu::ui::legacy

#endif
