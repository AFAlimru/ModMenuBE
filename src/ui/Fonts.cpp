#include "ui/Fonts.h"

#include "core/I18n.h"
#include "core/Log.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <imgui.h>
#include <imgui_internal.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
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

// Frequently used Traditional Chinese: Big5 level 1 (5401 characters).
constexpr ImWchar kCommonHanzi[] = {
#include "ui/CommonHanzi.inc"
};
void seedTraditional(std::set<ImWchar>& into) { into.insert(std::begin(kCommonHanzi), std::end(kCommonHanzi)); }

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

struct FontFace {
    char const* file;  // Windows: name under %WINDIR%\Fonts; Android: absolute path
    int         face;  // index inside a .ttc collection
};

std::filesystem::path fontFile(char const* name) {
#ifdef _WIN32
    wchar_t dir[MAX_PATH]{};
    GetWindowsDirectoryW(dir, MAX_PATH);
    return std::filesystem::path(dir) / L"Fonts" / name;
#else
    return name;
#endif
}

// Font files are read once and shared by every atlas rebuild (the atlas does not own them).
std::vector<char> const* fontData(std::filesystem::path const& path) {
    static std::map<std::filesystem::path, std::unique_ptr<std::vector<char>>> cache;
    auto& slot = cache[path];
    if (!slot) {
        slot = std::make_unique<std::vector<char>>();
        std::ifstream in(path, std::ios::binary);
        if (in) slot->assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    return slot->empty() ? nullptr : slot.get();
}

int faceCount(std::vector<char> const& d) {
    if (d.size() < 12) return 0;
    if (d[0] == 't' && d[1] == 't' && d[2] == 'c' && d[3] == 'f')
        return int((uint8_t(d[8]) << 24) | (uint8_t(d[9]) << 16) | (uint8_t(d[10]) << 8) | uint8_t(d[11]));
    return 1;
}

ImFont* addFont(ImFontAtlas* atlas, FontFace const* candidates, float size, ImWchar const* ranges, float weight) {
    for (auto const* c = candidates; c->file; ++c) {
        auto const* data = fontData(fontFile(c->file));
        if (!data) continue;
        int count = faceCount(*data);
        if (count <= 0) continue;
        ImFontConfig cfg;
        cfg.FontNo               = c->face < count ? c->face : 0;
        cfg.OversampleH          = 1;
        cfg.OversampleV          = 1;
        cfg.PixelSnapH           = true;
        cfg.RasterizerMultiply   = weight;
        cfg.FontDataOwnedByAtlas = false;
        if (ImFont* f = atlas->AddFontFromMemoryTTF(const_cast<char*>(data->data()), int(data->size()), size, &cfg,
                                                    ranges))
            return f;
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
#ifdef _WIN32
    static FontFace const kRegular[] = {{"msjh.ttc", 0}, {"msyh.ttc", 0}, {"segoeui.ttf", 0}, {}};
    static FontFace const kBold[]    = {{"msjhbd.ttc", 0}, {"msyhbd.ttc", 0}, {"segoeuib.ttf", 0}, {"msjh.ttc", 0}, {}};
    constexpr float       kBoldWeight = 1.0f;
#else
    // Android ships one CJK weight; face 3 of NotoSansCJK is Traditional Chinese. "Bold" is drawn heavier.
    static FontFace const kRegular[] = {{"/system/fonts/NotoSansCJK-Regular.ttc", 3},
                                        {"/system/fonts/NotoSansTC-Regular.otf", 0},
                                        {"/system/fonts/DroidSansFallbackFull.ttf", 0},
                                        {"/system/fonts/DroidSansFallback.ttf", 0},
                                        {}};
    static FontFace const kBold[]    = {{"/system/fonts/NotoSansCJK-Bold.ttc", 3}, {"/system/fonts/NotoSansCJK-Regular.ttc", 3},
                                        {"/system/fonts/NotoSansTC-Bold.otf", 0}, {"/system/fonts/NotoSansTC-Regular.otf", 0},
                                        {"/system/fonts/DroidSansFallbackFull.ttf", 0}, {}};
    constexpr float       kBoldWeight = 1.35f;
#endif
    gBody  = addFont(atlas, kRegular, std::round(kBodySize * scale), sRanges.data(), 1.0f);
    gBold  = addFont(atlas, kBold, std::round(kBodySize * scale), sShown.data(), kBoldWeight);
    gTitle = addFont(atlas, kBold, std::round(kTitleSize * scale), sShown.data(), kBoldWeight);
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
