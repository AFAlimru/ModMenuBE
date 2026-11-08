#include "ui/OreTheme.h"

#include "ui/Fonts.h"

#include <imgui_internal.h>

#include <algorithm>

namespace modmenu::ui::ore {

namespace {
float gScale = 1.0f;

ImVec4 v4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

struct Palette {
    ImU32 face, hover, lip, edge, text;
};

Palette palette(Variant v) {
    switch (v) {
    case Variant::Primary: return {color::Green, color::GreenHover, color::GreenLip, color::GreenEdge, color::Text};
    case Variant::Destructive: return {color::Red, color::RedHover, color::RedLip, color::rgb(0xE06464), color::Text};
    case Variant::Dark: return {color::Dark, color::DarkHover, color::DarkLip, color::PanelEdge, color::Text};
    case Variant::Secondary:
    default: return {color::Gray, color::GrayHover, color::GrayLip, color::GrayEdge, color::TextDark};
    }
}

// Shared drawing for buttons: outline, lip, face with highlight edge.
void drawButtonFrame(ImDrawList* dl, ImVec2 min, ImVec2 max, Palette const& p, bool hovered, bool held,
                     bool disabled, float lip) {
    float o = s(2);
    if (disabled) {
        dl->AddRectFilled(min, max, color::Outline);
        dl->AddRectFilled({min.x + o, min.y + o}, {max.x - o, max.y - o}, color::rgb(0x8C8D90));
        return;
    }
    float press = held ? lip : 0;
    dl->AddRectFilled(min, max, color::Outline);
    // lip
    dl->AddRectFilled({min.x + o, min.y + o}, {max.x - o, max.y - o}, p.lip);
    // face
    ImVec2 fmin{min.x + o, min.y + o + press};
    ImVec2 fmax{max.x - o, max.y - o - lip + press};
    dl->AddRectFilled(fmin, fmax, hovered ? p.hover : p.face);
    // inner light edge (top + left)
    float e = s(2);
    dl->AddRectFilled(fmin, {fmax.x, fmin.y + e}, p.edge);
    dl->AddRectFilled(fmin, {fmin.x + e, fmax.y}, p.edge);
}
} // namespace

float s(float v) { return v * gScale; }

void applyStyle(float scale) {
    gScale        = scale;
    ImGuiStyle& st = ImGui::GetStyle();
    st             = ImGuiStyle();
    st.WindowRounding = st.ChildRounding = st.FrameRounding = st.PopupRounding = st.ScrollbarRounding =
        st.GrabRounding = st.TabRounding = 0;
    st.WindowBorderSize = 0;
    st.ChildBorderSize  = 0;
    st.PopupBorderSize  = s(2);
    st.FrameBorderSize  = s(2);
    st.WindowPadding    = {s(12), s(12)};
    st.FramePadding     = {s(10), s(7)};
    st.ItemSpacing      = {s(8), s(8)};
    st.ItemInnerSpacing = {s(6), s(6)};
    st.ScrollbarSize    = s(14);
    st.GrabMinSize      = s(14);
    st.IndentSpacing    = s(18);

    ImVec4* c                       = st.Colors;
    c[ImGuiCol_Text]                = v4(color::Text);
    c[ImGuiCol_TextDisabled]        = v4(color::TextDim);
    c[ImGuiCol_WindowBg]            = v4(color::Panel);
    c[ImGuiCol_ChildBg]             = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]             = v4(color::PanelLight);
    c[ImGuiCol_Border]              = v4(color::Outline);
    c[ImGuiCol_FrameBg]             = v4(color::Inset);
    c[ImGuiCol_FrameBgHovered]      = v4(color::rgb(0x2E2E30));
    c[ImGuiCol_FrameBgActive]       = v4(color::rgb(0x2E2E30));
    c[ImGuiCol_ScrollbarBg]         = v4(color::Inset);
    c[ImGuiCol_ScrollbarGrab]       = v4(color::Gray);
    c[ImGuiCol_ScrollbarGrabHovered] = v4(color::GrayHover);
    c[ImGuiCol_ScrollbarGrabActive] = v4(color::GrayHover);
    c[ImGuiCol_CheckMark]           = v4(color::Text);
    c[ImGuiCol_SliderGrab]          = v4(color::Gray);
    c[ImGuiCol_SliderGrabActive]    = v4(color::Text);
    c[ImGuiCol_Button]              = v4(color::Dark);
    c[ImGuiCol_ButtonHovered]       = v4(color::DarkHover);
    c[ImGuiCol_ButtonActive]        = v4(color::DarkLip);
    c[ImGuiCol_Header]              = v4(color::PanelLight);
    c[ImGuiCol_HeaderHovered]       = v4(color::DarkHover);
    c[ImGuiCol_HeaderActive]        = v4(color::DarkHover);
    c[ImGuiCol_Separator]           = v4(color::Outline);
    c[ImGuiCol_TextSelectedBg]      = v4(color::rgb(0x3C8527, 160));
    c[ImGuiCol_NavHighlight]        = v4(color::Text);
    c[ImGuiCol_ModalWindowDimBg]    = v4(color::Dim);
}

void panel(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 fill, bool raised) {
    float o = s(2);
    dl->AddRectFilled(min, max, color::Outline);
    dl->AddRectFilled({min.x + o, min.y + o}, {max.x - o, max.y - o}, fill);
    if (raised) {
        dl->AddRectFilled({min.x + o, min.y + o}, {max.x - o, min.y + o + s(2)}, color::PanelEdge);
        dl->AddRectFilled({min.x + o, min.y + o}, {min.x + o + s(2), max.y - o}, color::PanelEdge);
    }
}

bool button(char const* id, char const* label, Variant variant, ImVec2 size, bool disabled) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    fonts::note(label);
    ImFont* font = fonts::bold() ? fonts::bold() : ImGui::GetFont();
    ImVec2 textSize = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0, label);
    float  lip      = s(4);
    if (size.x <= 0) size.x = textSize.x + s(32);
    if (size.y <= 0) size.y = textSize.y + s(16) + lip;
    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos, {pos.x + size.x, pos.y + size.y});
    ImGuiID gid = window->GetID(id);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, gid)) return false;
    bool hovered = false, held = false;
    bool pressed = !disabled && ImGui::ButtonBehavior(bb, gid, &hovered, &held);
    if (disabled) hovered = held = false;
    if (hovered && !disabled) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    Palette p = palette(variant);
    drawButtonFrame(window->DrawList, bb.Min, bb.Max, p, hovered, held, disabled, lip);
    float press = held ? lip : 0;
    ImVec2 tp{bb.Min.x + (size.x - textSize.x) * 0.5f, bb.Min.y + (size.y - lip - textSize.y) * 0.5f + press};
    window->DrawList->AddText(font, font->FontSize, tp, disabled ? color::rgb(0x4E4F52) : p.text, label);
    return pressed;
}

bool iconButton(char const* id, char const* glyph, Variant variant, float size, bool disabled) {
    return button(id, glyph, variant, {size, size}, disabled);
}

bool toggle(char const* id, bool* value, bool disabled) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;
    float  h   = s(30);
    float  w   = s(58);
    ImVec2 pos = window->DC.CursorPos;
    ImRect bb(pos, {pos.x + w, pos.y + h});
    ImGuiID gid = window->GetID(id);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, gid)) return false;
    bool hovered = false, held = false;
    bool pressed = !disabled && ImGui::ButtonBehavior(bb, gid, &hovered, &held);
    if (pressed) *value = !*value;
    if (hovered && !disabled) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImDrawList* dl = window->DrawList;
    float o = s(2);
    dl->AddRectFilled(bb.Min, bb.Max, color::Outline);
    ImU32 track = disabled ? color::rgb(0x58585A) : (*value ? color::Green : color::rgb(0x8C8D90));
    dl->AddRectFilled({bb.Min.x + o, bb.Min.y + o}, {bb.Max.x - o, bb.Max.y - o}, track);
    // tick / cross marks inside the track like OreUI
    float mid = bb.Min.y + h * 0.5f;
    if (*value) {
        float x = bb.Min.x + s(12);
        dl->AddRectFilled({x, mid - s(6)}, {x + s(2), mid + s(6)}, color::Text);
    } else {
        float x = bb.Max.x - s(16);
        dl->AddRect({x, mid - s(5)}, {x + s(9), mid + s(5)}, color::Text, 0, 0, s(2));
    }
    // knob
    float  kw   = s(26);
    float  kx   = *value ? bb.Max.x - kw : bb.Min.x;
    ImVec2 kmin{kx, bb.Min.y};
    ImVec2 kmax{kx + kw, bb.Max.y};
    dl->AddRectFilled(kmin, kmax, color::Outline);
    dl->AddRectFilled({kmin.x + o, kmin.y + o}, {kmax.x - o, kmax.y - o}, color::GrayLip);
    dl->AddRectFilled({kmin.x + o, kmin.y + o}, {kmax.x - o, kmax.y - o - s(4)},
                      hovered ? color::GrayHover : color::Gray);
    dl->AddRectFilled({kmin.x + o, kmin.y + o}, {kmax.x - o, kmin.y + o + s(2)}, color::GrayEdge);
    return pressed;
}

float badgeWidth(char const* text) {
    ImFont* f = fonts::bold() ? fonts::bold() : ImGui::GetFont();
    float   size = f->FontSize * 0.88f;
    return f->CalcTextSizeA(size, FLT_MAX, 0, text).x + s(12);
}

float badge(ImDrawList* dl, ImVec2 pos, char const* text, ImU32 fill, ImU32 textColor) {
    fonts::note(text);
    ImFont* f    = fonts::bold() ? fonts::bold() : ImGui::GetFont();
    float   size = f->FontSize * 0.88f;
    ImVec2  ts   = f->CalcTextSizeA(size, FLT_MAX, 0, text);
    ImVec2  max{pos.x + ts.x + s(12), pos.y + ts.y + s(4)};
    dl->AddRectFilled(pos, max, color::Outline);
    dl->AddRectFilled({pos.x + s(1), pos.y + s(1)}, {max.x - s(1), max.y - s(1)}, fill);
    dl->AddText(f, size, {pos.x + s(6), pos.y + s(2)}, textColor, text);
    return max.x - pos.x;
}

bool searchField(char const* id, char* buffer, size_t size, char const* hint, float width) {
    ImGui::PushStyleColor(ImGuiCol_Border, ImGui::ColorConvertU32ToFloat4(color::rgb(0x8C8D90)));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {s(10), s(9)});
    ImGui::SetNextItemWidth(width);
    bool changed = ImGui::InputTextWithHint(id, hint, buffer, size);
    if (ImGui::IsItemActive()) {
        ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), color::Text, 0, 0, s(2));
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    return changed;
}

void tooltip(char const* text) {
    if (!text || !*text || !ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) return;
    fonts::note(text);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {s(10), s(8)});
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(s(360));
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
    ImGui::PopStyleVar();
}

void text(char const* t, ImU32 c, ImFont* font) {
    fonts::note(t);
    if (font) ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(c));
    ImGui::TextUnformatted(t);
    ImGui::PopStyleColor();
    if (font) ImGui::PopFont();
}

void textWrapped(char const* t, ImU32 c, ImFont* font) {
    fonts::note(t);
    if (font) ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(c));
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(t);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    if (font) ImGui::PopFont();
}

} // namespace modmenu::ui::ore
