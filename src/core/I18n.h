#pragma once

#include <string>
#include <vector>

namespace modmenu {

// Returns the UI string for `key` in the active language (falls back to en_US, then the key).
char const* tr(char const* key);
// Resolves "auto" against the Windows UI language. Call when the setting changes.
void reloadLanguage();
std::string activeLanguage();
// Every string of every language, so the font atlas can include their glyphs.
std::vector<std::string> allStrings();

} // namespace modmenu
