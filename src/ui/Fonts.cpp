#include "ui/Fonts.h"

#include "core/I18n.h"
#include "core/Log.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <imgui.h>
#include <imgui_internal.h>

// The parser ImGui builds its atlas with: a font is only offered to ImGui when this accepts it, since one font
// it cannot parse fails the whole atlas (e.g. a variable/CFF2 CJK system font on newer Samsung phones).
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <imstb_truetype.h>

#include <algorithm>
#include <cctype>
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

// The face of `d` to load, preferring `wanted`, then any face with Chinese glyphs, then any face stb_truetype can
// parse (Latin-only fallbacks); -1 when it cannot parse the file at all.
int usableFace(std::vector<char> const& d, int wanted, bool needCjk) {
    if (d.size() < 12) return -1;
    auto const* p     = reinterpret_cast<unsigned char const*>(d.data());
    int         count = stbtt_GetNumberOfFonts(p);
    if (count <= 0) return -1;
    auto parses = [&](int i) {
        int            off = stbtt_GetFontOffsetForIndex(p, i);
        stbtt_fontinfo info{};
        return off >= 0 && size_t(off) < d.size() && stbtt_InitFont(&info, p, off) != 0;
    };
    auto hasCjk = [&](int i) {
        stbtt_fontinfo info{};
        stbtt_InitFont(&info, p, stbtt_GetFontOffsetForIndex(p, i));
        return stbtt_FindGlyphIndex(&info, 0x4E2D) != 0 && stbtt_FindGlyphIndex(&info, 0x9AD4) != 0; // 中 體
    };
    if (wanted < 0 || wanted >= count) wanted = 0;
    if (parses(wanted) && hasCjk(wanted)) return wanted;
    for (int i = 0; i < count; ++i)
        if (i != wanted && parses(i) && hasCjk(i)) return i;
    if (needCjk) return -1;
    return parses(wanted) ? wanted : -1;
}

void logChoice(std::string const& file, int face) {
    static std::set<std::string> logged;
    if (logged.insert(file + "#" + std::to_string(face)).second) log::info("font: {} (face {})", file, face);
}

#ifndef _WIN32
// Phones name their CJK fonts differently (and some ship only ones stb_truetype cannot parse): when none of the
// known files works, look through /system/fonts for any font with Chinese glyphs, Traditional ones first.
std::vector<std::string> const& scannedCjkFonts() {
    static std::vector<std::string> found = [] {
        std::vector<std::string> list;
        std::error_code          ec;
        for (auto const& e : std::filesystem::directory_iterator("/system/fonts", ec)) {
            std::string name = e.path().filename().string(), lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            bool font = lower.ends_with(".ttf") || lower.ends_with(".otf") || lower.ends_with(".ttc");
            bool cjk  = lower.find("cjk") != std::string::npos || lower.find("han") != std::string::npos
                    || lower.find("tc") != std::string::npos || lower.find("sc") != std::string::npos
                    || lower.find("chinese") != std::string::npos || lower.find("fallback") != std::string::npos;
            auto size = e.file_size(ec);
            if (font && cjk && !ec && size < (64u << 20)) list.push_back(e.path().string());
        }
        // Sans before serif (the UI is sans everywhere else), Traditional Chinese first within each.
        auto rank = [](std::string const& s) {
            bool serif = s.find("Serif") != std::string::npos || s.find("Ming") != std::string::npos
                      || s.find("Song") != std::string::npos;
            bool tc = s.find("TC") != std::string::npos || s.find("Hant") != std::string::npos;
            return (serif ? 2 : 0) + (tc ? 0 : 1);
        };
        std::stable_sort(list.begin(), list.end(), [&](auto const& a, auto const& b) { return rank(a) < rank(b); });
        std::string names;
        for (auto const& f : list) names += (names.empty() ? "" : ", ") + std::filesystem::path(f).filename().string();
        log::info("CJK font candidates in /system/fonts: {}", names.empty() ? "(none)" : names);
        return list;
    }();
    return found;
}
#endif

ImFont* addFontFile(ImFontAtlas* atlas, std::filesystem::path const& file, int wanted, bool needCjk, float size,
                    ImWchar const* ranges, float weight) {
    auto const* data = fontData(file);
    if (!data) return nullptr;
    int face = usableFace(*data, wanted, needCjk);
    if (face < 0) {
        static std::set<std::filesystem::path> warned;
        if (warned.insert(file).second) log::warn("font {} skipped: no usable face", file.string());
        return nullptr;
    }
    ImFontConfig cfg;
    cfg.FontNo               = face;
    cfg.OversampleH          = 1;
    cfg.OversampleV          = 1;
    cfg.PixelSnapH           = true;
    cfg.RasterizerMultiply   = weight;
    cfg.FontDataOwnedByAtlas = false;
    ImFont* f = atlas->AddFontFromMemoryTTF(const_cast<char*>(data->data()), int(data->size()), size, &cfg, ranges);
    if (f) logChoice(file.string(), face);
    return f;
}

ImFont* addFont(ImFontAtlas* atlas, FontFace const* candidates, float size, ImWchar const* ranges, float weight) {
    // Fonts with Chinese glyphs first; the Latin-only entries of the list only when none has them.
    for (bool needCjk : {true, false}) {
        for (auto const* c = candidates; c->file; ++c)
            if (ImFont* f = addFontFile(atlas, fontFile(c->file), c->face, needCjk, size, ranges, weight)) return f;
#ifndef _WIN32
        if (needCjk)
            for (auto const& file : scannedCjkFonts())
                if (ImFont* f = addFontFile(atlas, file, 0, true, size, ranges, weight)) return f;
#endif
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
    if (!atlas->Build()) {
        // Never leave a failed (0x0) atlas behind: drawing with it crashes. ImGui's built-in font always builds.
        log::error("font atlas build failed; falling back to the built-in font");
        atlas->Clear();
        ImFontConfig cfg;
        cfg.SizePixels = std::round(13.0f * scale);
        gBody = gBold = gTitle = atlas->AddFontDefault(&cfg);
        atlas->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
        if (!atlas->Build()) log::error("built-in font atlas failed too");
    }
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
