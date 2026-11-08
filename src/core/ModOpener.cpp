#include "core/ModOpener.h"

#include "core/Log.h"
#include "core/Util.h"

#include <Windows.h>

#include <cctype>
#include <vector>

namespace modmenu {

namespace {

struct NamedKey {
    char const* name;
    unsigned    vk;
};

constexpr NamedKey kNamed[] = {
    {"backspace", VK_BACK},      {"back", VK_BACK},          {"tab", VK_TAB},
    {"enter", VK_RETURN},        {"return", VK_RETURN},      {"pause", VK_PAUSE},
    {"capslock", VK_CAPITAL},    {"esc", VK_ESCAPE},         {"escape", VK_ESCAPE},
    {"space", VK_SPACE},         {"pageup", VK_PRIOR},       {"pgup", VK_PRIOR},
    {"pagedown", VK_NEXT},       {"pgdn", VK_NEXT},          {"end", VK_END},
    {"home", VK_HOME},           {"left", VK_LEFT},          {"up", VK_UP},
    {"right", VK_RIGHT},         {"down", VK_DOWN},          {"insert", VK_INSERT},
    {"ins", VK_INSERT},          {"delete", VK_DELETE},      {"del", VK_DELETE},
    {"numpadmultiply", VK_MULTIPLY}, {"numpadadd", VK_ADD},  {"numpadsubtract", VK_SUBTRACT},
    {"numpaddecimal", VK_DECIMAL},   {"numpaddivide", VK_DIVIDE}, {"numlock", VK_NUMLOCK},
    {"scrolllock", VK_SCROLL},   {"grave", VK_OEM_3},        {"`", VK_OEM_3},
    {"minus", VK_OEM_MINUS},     {"-", VK_OEM_MINUS},        {"equals", VK_OEM_PLUS},
    {"=", VK_OEM_PLUS},          {"[", VK_OEM_4},            {"]", VK_OEM_6},
    {";", VK_OEM_1},             {"'", VK_OEM_7},            {",", VK_OEM_COMMA},
    {".", VK_OEM_PERIOD},        {"/", VK_OEM_2},            {"\\", VK_OEM_5},
    {"mousemiddle", VK_MBUTTON}, {"middlemouse", VK_MBUTTON}, {"mbutton", VK_MBUTTON},
    {"mouse3", VK_MBUTTON},      {"mouseside1", VK_XBUTTON1}, {"mouse4", VK_XBUTTON1},
    {"xbutton1", VK_XBUTTON1},   {"mouseside2", VK_XBUTTON2}, {"mouse5", VK_XBUTTON2},
    {"xbutton2", VK_XBUTTON2},
};

std::string trim(std::string const& v) {
    size_t a = v.find_first_not_of(" \t");
    if (a == std::string::npos) return {};
    return v.substr(a, v.find_last_not_of(" \t") - a + 1);
}

std::optional<unsigned> parseKey(std::string const& raw) {
    std::string t = toLower(trim(raw));
    if (t.empty()) return std::nullopt;
    if (t.size() == 1 && std::isalnum(uint8_t(t[0]))) return unsigned(std::toupper(uint8_t(t[0])));
    if (t[0] == 'f' && t.size() <= 3 && std::isdigit(uint8_t(t[1]))) {
        int n = std::atoi(t.c_str() + 1);
        if (n >= 1 && n <= 24) return unsigned(VK_F1 + n - 1);
    }
    if (t.rfind("numpad", 0) == 0 && t.size() == 7 && std::isdigit(uint8_t(t[6]))) return unsigned(VK_NUMPAD0 + (t[6] - '0'));
    if (t.rfind("num", 0) == 0 && t.size() == 4 && std::isdigit(uint8_t(t[3]))) return unsigned(VK_NUMPAD0 + (t[3] - '0'));
    if (t.rfind("vk", 0) == 0) {
        std::string n = trim(t.substr(2));
        if (!n.empty() && (n[0] == '_' || n[0] == ' ')) n = trim(n.substr(1));
        char* end = nullptr;
        long  v   = std::strtol(n.c_str(), &end, 0);
        if (!n.empty() && end && *end == 0 && v > 0 && v < 256) return unsigned(v);
    }
    for (auto const& k : kNamed)
        if (t == k.name) return k.vk;
    return std::nullopt;
}

bool isExtended(unsigned vk) {
    switch (vk) {
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_PRIOR: case VK_NEXT: case VK_HOME:
    case VK_END: case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_NUMLOCK: case VK_RCONTROL: case VK_RMENU:
        return true;
    default: return false;
    }
}

INPUT keyInput(unsigned vk, bool up) {
    INPUT in{};
    if (vk == VK_MBUTTON || vk == VK_XBUTTON1 || vk == VK_XBUTTON2) {
        in.type = INPUT_MOUSE;
        if (vk == VK_MBUTTON) {
            in.mi.dwFlags = up ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN;
        } else {
            in.mi.dwFlags   = up ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN;
            in.mi.mouseData = vk == VK_XBUTTON1 ? XBUTTON1 : XBUTTON2;
        }
        return in;
    }
    in.type       = INPUT_KEYBOARD;
    in.ki.wVk     = WORD(vk);
    in.ki.wScan   = WORD(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0) | (isExtended(vk) ? KEYEVENTF_EXTENDEDKEY : 0);
    return in;
}

} // namespace

std::optional<Chord> parseChord(std::string const& text) {
    std::string t = trim(text);
    if (t.empty() || toLower(t) == "null" || toLower(t) == "none") return std::nullopt;
    Chord c;
    // Split on '+', but a trailing "+" key ("Ctrl++") is the plus key.
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= t.size()) {
        size_t plus = t.find('+', start);
        if (plus == start && plus + 1 == t.size()) {
            parts.push_back("=");
            break;
        }
        parts.push_back(t.substr(start, plus == std::string::npos ? std::string::npos : plus - start));
        if (plus == std::string::npos) break;
        start = plus + 1;
    }
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        auto m = toLower(trim(parts[i]));
        if (m == "ctrl" || m == "control") c.ctrl = true;
        else if (m == "alt") c.alt = true;
        else if (m == "shift") c.shift = true;
        else if (m == "win" || m == "meta" || m == "super") c.win = true;
        else return std::nullopt;
    }
    auto key = parseKey(parts.back());
    if (!key) return std::nullopt;
    c.vk = *key;
    return c;
}

std::string describeChord(Chord const& c) {
    std::string out;
    if (c.ctrl) out += "Ctrl+";
    if (c.alt) out += "Alt+";
    if (c.shift) out += "Shift+";
    if (c.win) out += "Win+";
    switch (c.vk) {
    case VK_MBUTTON: return out + "MouseMiddle";
    case VK_XBUTTON1: return out + "MouseSide1";
    case VK_XBUTTON2: return out + "MouseSide2";
    default: break;
    }
    if ((c.vk >= 'A' && c.vk <= 'Z') || (c.vk >= '0' && c.vk <= '9')) return out + char(c.vk);
    if (c.vk >= VK_F1 && c.vk <= VK_F24) return out + "F" + std::to_string(c.vk - VK_F1 + 1);
    LONG lp = LONG(MapVirtualKeyW(c.vk, MAPVK_VK_TO_VSC) << 16) | (isExtended(c.vk) ? (1 << 24) : 0);
    wchar_t name[64]{};
    if (GetKeyNameTextW(lp, name, 64) > 0) return out + toUtf8(name);
    return out + "VK_" + std::to_string(c.vk);
}

bool sendChord(Chord const& c, HWND__* window) {
    if (window && GetForegroundWindow() != window) SetForegroundWindow(window);
    std::vector<unsigned> mods;
    if (c.ctrl) mods.push_back(VK_CONTROL);
    if (c.alt) mods.push_back(VK_MENU);
    if (c.shift) mods.push_back(VK_SHIFT);
    if (c.win) mods.push_back(VK_LWIN);
    // Separate SendInput calls with short gaps: games and mods often sample
    // key state once per frame and miss a press and release in one batch.
    auto send = [](INPUT in) { return SendInput(1, &in, sizeof(INPUT)) == 1; };
    bool ok = true;
    for (auto m : mods) ok = send(keyInput(m, false)) && ok;
    Sleep(30);
    ok = send(keyInput(c.vk, false)) && ok;
    Sleep(60);
    ok = send(keyInput(c.vk, true)) && ok;
    Sleep(30);
    for (auto it = mods.rbegin(); it != mods.rend(); ++it) ok = send(keyInput(*it, true)) && ok;
    if (!ok) log::warn("SendInput failed for {} (error {})", describeChord(c), GetLastError());
    return ok;
}

} // namespace modmenu
