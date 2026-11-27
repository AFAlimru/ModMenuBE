#include "WorkspaceView.h"
#include "workspace/Service.h"
#include "workspace/ViewModel.h"
#include "ModMenu/TextEditor.h"
#include "core/I18n.h"
#include "ui/OreTheme.h"
#include "ui/Fonts.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
namespace modmenu::ui::workspaces {
namespace {
namespace ws = modmenu::workspace;
ws::ViewModel model;
ws::EntryDefinition routing;
std::atomic<bool> external{};
std::atomic<bool> waitingRelease{};
std::map<std::string, menu::WorkspaceJson> imageCache;
std::map<std::string, double> imageTime;
using J = menu::WorkspaceJson;
void text(std::string const& t, ImU32 color = ore::color::Text) {
    fonts::note(t);
    ore::textWrapped(t.c_str(), color);
}
std::string caption(menu::SettingTranslation const& t) {
    auto lang = activeLanguage();
    return lang == "zh_CN" && !t.cn.empty() ? t.cn : lang == "zh_TW" && !t.tw.empty() ? t.tw : t.en;
}
std::string jsonText(J const& j, char const* key) {
    return j.contains(key) ? caption(menu::WorkspaceCaption(j.at(key), 1024)) : std::string();
}
menu::WorkspacePage const* page(menu::WorkspaceSnapshot const& s, std::string const& id) {
    for (auto const& p : s.definition->pages)
        if (p.id == id)
            return &p;
    return nullptr;
}
bool targetReady(menu::WorkspaceSnapshot const& s, ws::Entry const& e) {
    return e.external ? ws::service().hasExternal(s.session, e.target) : s.state && page(s, e.target);
}
J values(menu::WorkspacePage const& p) {
    J v = J::object();
    for (auto const& c : p.controls) {
        if (c.type == "text")
            v[c.id] = model.text[c.id];
        else if (c.type == "boolean")
            v[c.id] = model.booleans[c.id];
    }
    return v;
}
void submit(menu::WorkspaceSnapshot const& s, menu::WorkspacePage const& p, std::string const& a,
            std::uint64_t revision, std::string const& row) {
    std::string error;
    if (!ws::service().submit(s.session, p.id, a, revision, values(p), row, error))
        model.message = error;
    else
        model.message = tr("workspace.submitted");
}
void drawImage(std::string const& id, J const& state) {
    if (!state.contains("pixels"))
        return;
    auto now = ImGui::GetTime();
    if (!imageCache.count(id) || now - imageTime[id] >= .25) {
        imageCache[id] = state;
        imageTime[id] = now;
    }
    auto const& image = imageCache[id];
    unsigned w = image.at("width"), h = image.at("height");
    float scale = std::max(1.f, std::min(ore::s(4), ImGui::GetContentRegionAvail().x / float(w)));
    auto pos = ImGui::GetCursorScreenPos();
    auto dl = ImGui::GetWindowDrawList();
    for (unsigned y = 0; y < h; ++y)
        for (unsigned x = 0; x < w; ++x) {
            auto pixel = image["pixels"][y * w + x].get<std::uint32_t>();
            auto color = IM_COL32((pixel >> 24) & 255, (pixel >> 16) & 255, (pixel >> 8) & 255, pixel & 255);
            dl->AddRectFilled({pos.x + x * scale, pos.y + y * scale},
                              {pos.x + (x + 1) * scale, pos.y + (y + 1) * scale}, color);
        }
    ImGui::Dummy({w * scale, h * scale});
}
void initForm(menu::WorkspacePage const& p, J const& states) {
    for (auto const& c : p.controls) {
        auto state = states.value(c.id, J::object());
        if (c.type == "text" && !model.text.count(c.id)) {
            model.text[c.id] = state.value("value", std::string());
            model.committed[c.id] = model.text[c.id];
        }
        if (c.type == "boolean" && !model.booleans.count(c.id))
            model.booleans[c.id] = state.value("value", false);
    }
}
} // namespace
bool available(ModInfo const& m) {
    return m.loaded && m.adapter && m.adapter->workspace.visible();
}
bool open(ModInfo const& m) {
    if (!available(m))
        return false;
    close();
    routing = m.adapter->workspace;
    auto s = ws::service().snapshot(m.name);
    model.open(m.name, s ? s->session : 0);
    return true;
}
void releaseResources() {
    imageCache.clear();
    imageTime.clear();
}
void suspend() {
    model.suspend();
    if (ImGui::GetCurrentContext())
        ImGui::ClearActiveID();
}
void close() {
    auto h = ws::service().handoff(ws::milliseconds());
    if (h.session == model.session && h.request) {
        ws::service().cancelExternal(h.request);
        ws::service().finishExternal(h.request);
    }
    external = false;
    waitingRelease = false;
    model = {};
    routing = {};
    releaseResources();
}
bool back() {
    return model.back();
}
bool externalActive() {
    return external.load();
}
bool inputSuspended() {
    return external.load() || waitingRelease.load();
}
bool tick(bool origin, bool foreground, bool released) {
    if (model.mod.empty())
        return true;
    auto snapshot = ws::service().snapshot(model.mod);
    if (!origin || (model.session && (!snapshot || snapshot->session != model.session))) {
        close();
        return false;
    }
    if (!model.session && snapshot)
        model.observe(snapshot->session);
    auto h = ws::service().handoff(ws::milliseconds());
    if (h.session != model.session || h.phase == ws::HandoffPhase::None)
        return true;
    if (h.phase == ws::HandoffPhase::Prepared) {
        waitingRelease = true;
        suspend();
        if (foreground && released) {
            external = true;
            waitingRelease = false;
            ws::service().releaseExternal(h.session, h.request, ws::milliseconds());
        }
    } else if (h.phase == ws::HandoffPhase::Returned || h.phase == ws::HandoffPhase::Failed) {
        external = false;
        waitingRelease = false;
        suspend();
        model.message = h.phase == ws::HandoffPhase::Returned ? tr("workspace.returned") : h.message;
        ws::service().finishExternal(h.request);
    }
    return true;
}
void draw(float height) {
    auto snapshot = ws::service().snapshot(model.mod);
    if (!snapshot) {
        text(tr("workspace.unavailable"), ore::color::Yellow);
        for (auto const& e : routing.entries) {
            auto label = e.label.get(activeLanguage());
            fonts::note(label);
            ore::button(e.id.c_str(), label.c_str(), ore::Variant::Dark,
                        {std::max(1.f, ImGui::GetContentRegionAvail().x - ore::s(4)), ore::s(44)}, true);
        }
        return;
    }
    if (!model.observe(snapshot->session)) {
        releaseResources();
    }
    auto const& s = *snapshot;
    text(tr("workspace.intro"), ore::color::TextDim);
    if (!model.message.empty())
        text(model.message, ore::color::Yellow);
    if (model.page.empty()) {
        ImGui::BeginChild("workspace-entry-hub", {0, std::max(1.f, height - ore::s(80))});
        for (auto const& e : routing.entries) {
            ImGui::PushID(e.id.c_str());
            bool ready = targetReady(s, e);
            auto label = e.label.get(activeLanguage());
            fonts::note(label);
            if (ore::button("##entry", label.c_str(), e.external ? ore::Variant::Dark : ore::Variant::Primary,
                            {std::max(1.f, ImGui::GetContentRegionAvail().x - ore::s(4)), ore::s(44)},
                            !ready)) {
                if (e.external) {
                    auto r = ws::service().beginExternal(s.session, e.target, ws::milliseconds());
                    if (!r)
                        model.message = tr("workspace.busy");
                    else
                        model.message = tr("workspace.handoff");
                } else {
                    model.page = e.target;
                    model.message.clear();
                }
            }
            text(!ready ? tr("workspace.target_unavailable")
                        : tr(e.external ? "workspace.external_hint" : "workspace.page_hint"),
                 ore::color::TextDim);
            ImGui::Dummy({0, ore::s(8)});
            ImGui::PopID();
        }
        ImGui::EndChild();
        return;
    }
    auto pg = page(s, model.page);
    if (!pg || !s.state) {
        model.page.clear();
        return;
    }
    // Only explicitly declared page targets appear in navigation.
    ImGui::BeginChild("workspace-tabs", {0, ore::s(52)}, ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    bool first = true;
    for (auto const& e : routing.entries)
        if (!e.external && page(s, e.target)) {
            if (!first)
                ImGui::SameLine();
            first = false;
            auto label = e.label.get(activeLanguage());
            fonts::note(label);
            if (ore::button(e.id.c_str(), label.c_str(),
                            model.page == e.target ? ore::Variant::Primary : ore::Variant::Dark)) {
                model.suspend();
                model.page = e.target;
            }
        }
    ImGui::EndChild();
    pg = page(s, model.page);
    if (!pg)
        return;
    auto const& states = s.state->at("controls");
    initForm(*pg, states);
    auto revision = s.state->at("revision").get<std::uint64_t>();
    auto childId = "workspace-page-" + model.page;
    ImGui::BeginChild(childId.c_str(), {0, std::max(1.f, height - ore::s(130))});
    for (auto const& c : pg->controls) {
        ImGui::PushID(c.id.c_str());
        text(caption(c.label));
        auto state = states.value(c.id, J::object());
        bool enabled = states.contains(c.id) && state.value("enabled", true) && !state.value("busy", false);
        bool pending = std::find(s.pending.begin(), s.pending.end(),
                                 c.type == "list" ? c.selectAction : c.id) != s.pending.end();
        ImGui::BeginDisabled(!enabled || pending);
        if (c.type == "text") {
            std::array<char, 4097> buffer{};
            auto& value = model.text[c.id];
            std::memcpy(buffer.data(), value.data(), std::min(value.size(), buffer.size() - 1));
            ImGui::SetNextItemWidth(-1);
            if (ImGui::InputText("##text", buffer.data(), buffer.size(),
                                 ImGuiInputTextFlags_EnterReturnsTrue)) {
                value = buffer.data();
                model.committed[c.id] = value;
                model.editing.clear();
            }
            if (ImGui::IsItemActive()) {
                value = buffer.data();
                model.editing = c.id;
                fonts::note(value);
            }
            if (ImGui::IsItemDeactivated()) {
                value = buffer.data();
                model.committed[c.id] = value;
                if (model.editing == c.id)
                    model.editing.clear();
            }
            if (value.size() < c.minBytes || !menu::PlainText(value, c.minBytes, c.maxBytes, c.identifier))
                text(tr("workspace.invalid_text"), ore::color::Red);
        } else if (c.type == "boolean") {
            bool v = model.booleans[c.id];
            if (ore::toggle("##bool", &v, !enabled || pending))
                model.booleans[c.id] = v;
        } else if (c.type == "action") {
            if (ore::button("##action", caption(c.label).c_str(), ore::Variant::Primary,
                            {std::max(1.f, ImGui::GetContentRegionAvail().x - ore::s(4)), 0},
                            !enabled || pending)) {
                if (!c.confirmation.en.empty()) {
                    model.confirmAction = c.id;
                    model.confirmRevision = revision;
                    ImGui::OpenPopup("##workspace-confirm");
                } else
                    submit(s, *pg, c.id, revision, {});
            }
        } else if (c.type == "progress") {
            double total = state.value("total", std::uint64_t(1));
            double v = state.value("value", std::uint64_t(0));
            ImGui::ProgressBar(float(v / std::max(1., total)), {-1, ore::s(24)});
        } else if (c.type == "list") {
            auto rows = state.value("rows", J::array());
            if (rows.empty())
                text(c.emptyText.en.empty() ? tr("workspace.empty") : caption(c.emptyText),
                     ore::color::TextDim);
            ImGui::BeginChild("##rows", {0, ore::s(std::min(5, std::max(1, int(rows.size()))) * 64)},
                              ImGuiChildFlags_Borders);
            for (auto const& r : rows) {
                auto id = r.at("id").get<std::string>();
                ImGui::PushID(id.c_str());
                auto label = jsonText(r, "label");
                fonts::note(label);
                bool active = r.value("enabled", true);
                ImGui::BeginDisabled(!active);
                if (ImGui::Selectable(label.c_str(), model.selectedRows[c.id] == id)) {
                    model.selectedRows[c.id] = id;
                    if (!c.selectAction.empty())
                        submit(s, *pg, c.selectAction, revision, id);
                }
                ImGui::EndDisabled();
                auto info = jsonText(r, "text");
                if (!info.empty())
                    text(info, ore::color::TextDim);
                auto reason = jsonText(r, "reason");
                if (!reason.empty())
                    text(reason, ore::color::Yellow);
                ImGui::PopID();
            }
            ImGui::EndChild();
        } else if (c.type == "image")
            drawImage(c.id, state);
        ImGui::EndDisabled();
        auto info = jsonText(state, "text");
        if (!info.empty())
            text(info, c.type == "status" ? ore::color::Text : ore::color::TextDim);
        for (auto key : {"reason", "error"}) {
            auto message = jsonText(state, key);
            if (!message.empty())
                text(message, key == std::string("error") ? ore::color::Red : ore::color::Yellow);
        }
        if (pending || state.value("busy", false))
            text(tr("workspace.waiting"), ore::color::TextDim);
        if (c.type == "action" &&
            ImGui::BeginPopupModal("##workspace-confirm", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if (model.confirmAction != c.id) {
                ImGui::CloseCurrentPopup();
            } else {
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ore::s(350));
                text(caption(c.confirmation));
                ImGui::PopTextWrapPos();
                if (ore::button("##confirm", tr("workspace.confirm"), ore::Variant::Destructive)) {
                    submit(s, *pg, c.id, model.confirmRevision, {});
                    model.confirmAction.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ore::button("##cancel", tr("cancel")) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    model.confirmAction.clear();
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    if (s.state->contains("results"))
        for (auto const& r : s.state->at("results")) {
            auto message = jsonText(r, "message");
            if (!message.empty())
                text(message, r.at("state") == "failed" ? ore::color::Red : ore::color::TextDim);
        }
    model.scroll[model.page] = ImGui::GetScrollY();
    ImGui::EndChild();
}
} // namespace modmenu::ui::workspaces
