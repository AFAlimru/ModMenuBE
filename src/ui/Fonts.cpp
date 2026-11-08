#include "ui/Fonts.h"

#include "core/I18n.h"
#include "core/Log.h"

#include <Windows.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <filesystem>
#include <mutex>
#include <set>
#include <vector>

namespace modmenu::ui::fonts {

namespace {

std::mutex          gMutex;
std::set<ImWchar>   gKnown;     // body font: displayed text + common Traditional Chinese
std::set<ImWchar>   gShown;     // bold/title fonts: only text that was actually displayed
bool                gDirty = true;
float               gScale = 0;
ImFont*             gBody{};
ImFont*             gBold{};
ImFont*             gTitle{};
bool                gBaseSeeded = false;

constexpr float kBodySize  = 18.0f;
constexpr float kTitleSize = 26.0f;

void addUtf8(std::string_view text, std::set<ImWchar>& into, bool& added) {
    char const* p   = text.data();
    char const* end = p + text.size();
    while (p < end) {
        unsigned int c = 0;
        int          n = ImTextCharFromUtf8(&c, p, end);
        if (n <= 0) break;
        p += n;
        if (c < 0x20 || c > 0xFFFF) continue; // ImWchar is 16-bit in this build
        if (into.insert(ImWchar(c)).second) added = true;
    }
}

// Frequently used Traditional Chinese: Big5 level 1 (5401 characters), via code page 950.
void seedTraditional(std::set<ImWchar>& into) {
    char buf[2];
    for (int lead = 0xA4; lead <= 0xC6; ++lead) {
        for (int trail : {0x40, 0xA1}) {
            int last = trail == 0x40 ? 0x7E : 0xFE;
            for (int t = trail; t <= last; ++t) {
                if (lead == 0xC6 && t > 0x7E) break;
                buf[0] = char(lead);
                buf[1] = char(t);
                wchar_t w = 0;
                if (MultiByteToWideChar(950, MB_ERR_INVALID_CHARS, buf, 2, &w, 1) == 1 && w >= 0x4E00) into.insert(w);
            }
        }
    }
}

void seedBase() {
    for (ImWchar c = 0x20; c < 0x250; ++c) gKnown.insert(c), gShown.insert(c);
    // General punctuation, arrows, CJK symbols, full-width forms, a few dingbats.
    for (ImWchar c = 0x2010; c <= 0x2027; ++c) gKnown.insert(c);
    for (ImWchar c = 0x2190; c <= 0x2199; ++c) gKnown.insert(c);
    for (ImWchar c = 0x3000; c <= 0x301F; ++c) gKnown.insert(c);
    for (ImWchar c = 0xFF01; c <= 0xFF5E; ++c) gKnown.insert(c);
    for (ImWchar c : {0x2022, 0x22EF, 0x2713, 0x2715, 0x25B2, 0x25BC, 0x25B6, 0x2026}) gKnown.insert(c), gShown.insert(c);
    seedTraditional(gKnown);
    bool added = false;
    for (auto const& s : allStrings()) addUtf8(s, gKnown, added), addUtf8(s, gShown, added);
}

std::filesystem::path fontFile(wchar_t const* name) {
    wchar_t dir[MAX_PATH]{};
    GetWindowsDirectoryW(dir, MAX_PATH);
    return std::filesystem::path(dir) / L"Fonts" / name;
}

ImFont* addFont(ImFontAtlas* atlas, wchar_t const* const* candidates, float size, ImWchar const* ranges) {
    for (auto const* const* c = candidates; *c; ++c) {
        auto path = fontFile(*c);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) continue;
        ImFontConfig cfg;
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        cfg.PixelSnapH  = true;
        if (ImFont* f = atlas->AddFontFromFileTTF(path.string().c_str(), size, &cfg, ranges)) return f;
    }
    return nullptr;
}

} // namespace

void note(std::string_view utf8) {
    std::lock_guard lock(gMutex);
    bool added = false;
    addUtf8(utf8, gKnown, added);
    addUtf8(utf8, gShown, added);
    if (added) gDirty = true;
}

namespace {
std::vector<ImWchar> toRanges(std::set<ImWchar> const& set) {
    std::vector<ImWchar> ranges;
    for (auto it = set.begin(); it != set.end();) {
        ImWchar first = *it, last = *it;
        ++it;
        while (it != set.end() && *it == last + 1) last = *it++;
        ranges.push_back(first);
        ranges.push_back(last);
    }
    ranges.push_back(0);
    return ranges;
}
} // namespace

bool update(float scale) {
    std::vector<ImWchar> ranges, shownRanges;
    {
        std::lock_guard lock(gMutex);
        if (!gBaseSeeded) {
            seedBase();
            gBaseSeeded = true;
        }
        if (!gDirty && std::abs(scale - gScale) < 0.01f && gBody) return false;
        ranges      = toRanges(gKnown);
        shownRanges = toRanges(gShown);
        gDirty      = false;
        gScale = scale;
    }

    static std::vector<ImWchar> sRanges, sShown; // must outlive the atlas
    sRanges = std::move(ranges);
    sShown  = std::move(shownRanges);

    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    atlas->Clear();
    static wchar_t const* const kRegular[] = {L"msjh.ttc", L"msyh.ttc", L"segoeui.ttf", nullptr};
    static wchar_t const* const kBold[]    = {L"msjhbd.ttc", L"msyhbd.ttc", L"segoeuib.ttf", L"msjh.ttc", nullptr};
    gBody  = addFont(atlas, kRegular, std::round(kBodySize * scale), sRanges.data());
    gBold  = addFont(atlas, kBold, std::round(kBodySize * scale), sShown.data());
    gTitle = addFont(atlas, kBold, std::round(kTitleSize * scale), sShown.data());
    if (!gBody) gBody = atlas->AddFontDefault();
    if (!gBold) gBold = gBody;
    if (!gTitle) gTitle = gBold;
    atlas->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
    if (!atlas->Build()) log::error("font atlas build failed");
    ImGui::GetIO().FontDefault = gBody;
    log::info("font atlas rebuilt: {} glyph ranges, scale {:.2f}, {}x{}", sRanges.size() / 2, scale, atlas->TexWidth,
              atlas->TexHeight);
    return true;
}

ImFont* body() { return gBody; }
ImFont* bold() { return gBold; }
ImFont* title() { return gTitle; }
float   scale() { return gScale; }

} // namespace modmenu::ui::fonts
