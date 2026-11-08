#pragma once

#include <string_view>

struct ImFont;

namespace modmenu::ui::fonts {

// Records text that will be drawn so its glyphs get into the atlas.
// Cheap when every code point is already known.
void note(std::string_view utf8);

// Rebuilds the atlas when new glyphs were noted or the scale changed.
// Returns true when the atlas texture must be re-uploaded.
bool update(float scale);

ImFont* body();
ImFont* bold();
ImFont* title();
float   scale();

} // namespace modmenu::ui::fonts
