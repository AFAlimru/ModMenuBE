#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace modmenu {

std::string  toUtf8(std::wstring_view text);
std::wstring fromUtf8(std::string_view text);
std::string  pathUtf8(std::filesystem::path const& path);

std::string toLower(std::string_view text);
// Case-insensitive substring search on UTF-8 (ASCII folding only).
bool containsIgnoreCase(std::string_view haystack, std::string_view needle);

// Compares dotted versions ("v1.2.10-beta" < "1.3"). Non-numeric suffixes are
// ignored. Returns <0, 0, >0.
int compareVersions(std::string_view a, std::string_view b);
// "26.60.5" -> "26.60"
std::string majorMinor(std::string_view version);

bool readFile(std::filesystem::path const& path, std::string& out);
bool writeFileAtomic(std::filesystem::path const& path, std::string_view data);

// Opens a folder in Explorer or an http(s) URL in the browser. Other schemes are refused.
bool openExternal(std::string const& target);
bool openFolder(std::filesystem::path const& folder);

} // namespace modmenu
