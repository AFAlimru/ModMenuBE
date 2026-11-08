#pragma once

#include <imgui.h>

// OreUI look for ImGui: flat square panels with a 2px dark outline, a light
// top/left inner edge, and buttons that carry a darker "lip" under the face
// that collapses while pressed — the style of Bedrock's new settings screens.
namespace modmenu::ui::ore {

namespace color {
constexpr ImU32 rgb(unsigned hex, unsigned a = 255) {
    return IM_COL32((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, a);
}
inline constexpr ImU32 Outline      = rgb(0x1E1E1F);
inline constexpr ImU32 Panel        = rgb(0x313233);
inline constexpr ImU32 PanelLight   = rgb(0x48494A);
inline constexpr ImU32 PanelEdge    = rgb(0x5A5B5C);
inline constexpr ImU32 Inset        = rgb(0x242425);
inline constexpr ImU32 Text         = rgb(0xFFFFFF);
inline constexpr ImU32 TextDim      = rgb(0xB1B2B5);
inline constexpr ImU32 TextDark     = rgb(0x1E1E1F);
inline constexpr ImU32 Green        = rgb(0x3C8527);
inline constexpr ImU32 GreenHover   = rgb(0x2A641C);
inline constexpr ImU32 GreenLip     = rgb(0x1D4D13);
inline constexpr ImU32 GreenEdge    = rgb(0x6CC349);
inline constexpr ImU32 Gray         = rgb(0xD0D1D4);
inline constexpr ImU32 GrayHover    = rgb(0xB1B2B5);
inline constexpr ImU32 GrayLip      = rgb(0x58585A);
inline constexpr ImU32 GrayEdge     = rgb(0xECECEC);
inline constexpr ImU32 Dark         = rgb(0x48494A);
inline constexpr ImU32 DarkHover    = rgb(0x5A5B5C);
inline constexpr ImU32 DarkLip      = rgb(0x313233);
inline constexpr ImU32 Red          = rgb(0xC33636);
inline constexpr ImU32 RedHover     = rgb(0xA12B2B);
inline constexpr ImU32 RedLip       = rgb(0x6E1A1A);
inline constexpr ImU32 Yellow       = rgb(0xFFE866);
inline constexpr ImU32 Blue         = rgb(0x2E6BE5);
inline constexpr ImU32 Purple       = rgb(0x8B5CF6);
inline constexpr ImU32 Selection    = rgb(0xFFFFFF);
inline constexpr ImU32 Dim          = rgb(0x000000, 150);
} // namespace color

enum class Variant { Primary, Secondary, Dark, Destructive };

// Applies the palette and metrics to ImGui's style (call when the scale changes).
void applyStyle(float scale);
float s(float v); // scales a design pixel value

void panel(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 fill = color::Panel, bool raised = true);

bool button(char const* id, char const* label, Variant variant = Variant::Secondary, ImVec2 size = {0, 0},
            bool disabled = false);
// Square icon-only button (e.g. close "x").
bool iconButton(char const* id, char const* glyph, Variant variant, float size, bool disabled = false);
bool toggle(char const* id, bool* value, bool disabled = false);
// A small coloured chip; returns its width.
float badge(ImDrawList* dl, ImVec2 pos, char const* text, ImU32 fill, ImU32 textColor = color::Text);
float badgeWidth(char const* text);

// ImGui input/slider/combo wrapped with OreUI framing.
bool searchField(char const* id, char* buffer, size_t size, char const* hint, float width);
void tooltip(char const* text);

// Text with explicit color and font.
void text(char const* text, ImU32 color = color::Text, ImFont* font = nullptr);
void textWrapped(char const* text, ImU32 color = color::Text, ImFont* font = nullptr);

} // namespace modmenu::ui::ore
