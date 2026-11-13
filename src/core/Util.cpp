#include "core/Util.h"

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#endif

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <vector>

namespace modmenu {

#ifdef _WIN32
std::string toUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(size_t(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring fromUtf8(std::string_view text) {
    if (text.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
    std::wstring out(size_t(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size);
    return out;
}

std::string pathUtf8(std::filesystem::path const& path) { return toUtf8(path.wstring()); }
#else
// wchar_t is UTF-32 here.
std::string toUtf8(std::wstring_view text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t wc : text) {
        auto c = uint32_t(wc);
        if (c < 0x80) out += char(c);
        else if (c < 0x800) out += char(0xC0 | (c >> 6)), out += char(0x80 | (c & 0x3F));
        else if (c < 0x10000)
            out += char(0xE0 | (c >> 12)), out += char(0x80 | ((c >> 6) & 0x3F)), out += char(0x80 | (c & 0x3F));
        else
            out += char(0xF0 | (c >> 18)), out += char(0x80 | ((c >> 12) & 0x3F)),
                out += char(0x80 | ((c >> 6) & 0x3F)), out += char(0x80 | (c & 0x3F));
    }
    return out;
}

std::wstring fromUtf8(std::string_view text) {
    std::wstring out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        auto     b = uint8_t(text[i]);
        uint32_t c = 0;
        int      n = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 1;
        if (n == 1) c = b < 0x80 ? b : 0xFFFD;
        else {
            c = b & (0xFF >> (n + 1));
            for (int k = 1; k < n && i + k < text.size(); ++k) c = (c << 6) | (uint8_t(text[i + k]) & 0x3F);
        }
        out += wchar_t(c);
        i += size_t(n);
    }
    return out;
}

std::string pathUtf8(std::filesystem::path const& path) { return path.string(); }
#endif

std::string toLower(std::string_view text) {
    std::string out(text);
    for (auto& c : out) {
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    }
    return out;
}

bool containsIgnoreCase(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    return toLower(haystack).find(toLower(needle)) != std::string::npos;
}

namespace {
std::vector<long long> versionParts(std::string_view v) {
    if (!v.empty() && (v.front() == 'v' || v.front() == 'V')) v.remove_prefix(1);
    // Stop at pre-release / build metadata.
    size_t end = v.find_first_of("-+ ");
    if (end != std::string_view::npos) v = v.substr(0, end);
    std::vector<long long> parts;
    long long current = 0;
    bool      any     = false;
    for (char c : v) {
        if (c >= '0' && c <= '9') {
            current = current * 10 + (c - '0');
            any     = true;
        } else if (c == '.') {
            parts.push_back(current);
            current = 0;
            any     = false;
        } else {
            break;
        }
    }
    if (any || !parts.empty()) parts.push_back(current);
    while (parts.size() > 1 && parts.back() == 0) parts.pop_back();
    return parts;
}
} // namespace

int compareVersions(std::string_view a, std::string_view b) {
    auto pa = versionParts(a);
    auto pb = versionParts(b);
    size_t n = std::max(pa.size(), pb.size());
    for (size_t i = 0; i < n; ++i) {
        long long x = i < pa.size() ? pa[i] : 0;
        long long y = i < pb.size() ? pb[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

std::string majorMinor(std::string_view version) {
    if (!version.empty() && (version.front() == 'v' || version.front() == 'V')) version.remove_prefix(1);
    size_t first = version.find('.');
    if (first == std::string_view::npos) return std::string(version);
    size_t second = version.find('.', first + 1);
    return std::string(version.substr(0, second));
}

bool readFile(std::filesystem::path const& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    // Strip a UTF-8 BOM, LeviLauncher writes some files with one.
    if (out.size() >= 3 && uint8_t(out[0]) == 0xEF && uint8_t(out[1]) == 0xBB && uint8_t(out[2]) == 0xBF) {
        out.erase(0, 3);
    }
    return true;
}

bool writeFileAtomic(std::filesystem::path const& path, std::string_view data) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    auto tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(data.data(), std::streamsize(data.size()));
        if (!out) return false;
    }
#ifdef _WIN32
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
#else
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
#endif
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

#ifdef _WIN32
bool openExternal(std::string const& target) {
    std::string lower = toLower(target);
    if (lower.rfind("https://", 0) != 0 && lower.rfind("http://", 0) != 0) return false;
    auto wide = fromUtf8(target);
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
}

bool openFolder(std::filesystem::path const& folder) {
    return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL))
         > 32;
}
#endif // openExternal / openFolder live in the Android platform layer

} // namespace modmenu
