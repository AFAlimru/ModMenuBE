// The mod list screen (Java ModMenu's ModsScreen) drawn with the OreUI theme.
#include "ui/Ui.h"
#include "ui/MenuControl.h"

#include "core/ConfigRegistry.h"
#include "core/I18n.h"
#include "core/JsonConfigDoc.h"
#include "core/LipCatalog.h"
#include "core/ModOpener.h"
#include "core/Log.h"
#include "core/ModRegistry.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include "core/UpdateChecker.h"
#include "core/Util.h"
#include "overlay/Overlay.h"
#include "ui/Fonts.h"
#include "ui/IconCache.h"
#include "ui/Markdown.h"
#include "ui/OreTheme.h"
#include "ui/SafeCall.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <map>
#include <mutex>
#include <set>
#include <thread>

namespace modmenu::ui {

namespace {

using ore::s;
namespace col = ore::color;

// ---------------------------------------------------------------- requests
struct Requests {
    bool        open = false, close = false, toggle = false, rescan = false;
    std::string selectMod;
    bool        openConfig = false;
};
std::mutex        gRequestMutex;
Requests          gRequests;
std::atomic<bool> gOpen{false};
std::atomic<bool> gPending{false};
std::atomic<bool> gCapturingHotkey{false};

// ---------------------------------------------------------------- screen state (render thread)
enum class View { List, Config, Json };

struct State {
    View        view = View::List;
    char        search[128]{};
    std::string selected;
    std::string configMod;            // mod whose config is open
    int         configCategory = 0;
    std::vector<JsonConfigDoc> docs;  // JSON editor
    int         docIndex = 0;
    std::map<std::string, std::vector<char>> stringBuffers;
    std::string toast;
    double      toastUntil = 0;
    bool        confirmDisableLL = false;
    bool        checkedUpdatesThisSession = false;
    float       appliedScale = 0;
    std::set<std::string> faultedScreens; // config screens whose callbacks crashed
} gState;

std::string fmt(char const* format, ...) {
    char    buf[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return buf;
}

void showToast(std::string text) {
    gState.toast      = std::move(text);
    gState.toastUntil = ImGui::GetTime() + 4.0;
}

std::string keyName(int vk) {
    UINT scan = MapVirtualKeyW(UINT(vk), MAPVK_VK_TO_VSC);
    LONG lp   = LONG(scan << 16);
    if (vk == VK_INSERT || vk == VK_DELETE || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT
        || (vk >= VK_LEFT && vk <= VK_DOWN))
        lp |= 1 << 24;
    wchar_t name[64]{};
    if (GetKeyNameTextW(lp, name, 64) > 0) return toUtf8(name);
    return "VK " + std::to_string(vk);
}

// ---------------------------------------------------------------- helpers
ImU32 tileColor(std::string const& name) {
    static ImU32 const palette[] = {col::Green, col::Blue, col::Purple, col::rgb(0xC97B26), col::rgb(0x1E8A8A),
                                    col::rgb(0xB0306A)};
    unsigned h = 2166136261u;
    for (char c : name) h = (h ^ uint8_t(c)) * 16777619u;
    return palette[h % (sizeof(palette) / sizeof(palette[0]))];
}

std::filesystem::path iconFor(ModInfo const& mod);

void drawIcon(ImDrawList* dl, ModInfo const& mod, ImVec2 min, float size) {
    ImVec2 max{min.x + size, min.y + size};
    dl->AddRectFilled(min, max, col::Outline);
    ImVec2 imin{min.x + s(2), min.y + s(2)}, imax{max.x - s(2), max.y - s(2)};
    if (ImTextureID tex = icons::get(iconFor(mod))) {
        dl->AddImage(tex, imin, imax);
        return;
    }
    dl->AddRectFilled(imin, imax, tileColor(mod.name));
    // First character of the name.
    std::string letter;
    if (!mod.name.empty()) {
        unsigned    c   = 0;
        char const* beg = mod.name.c_str();
        int         n   = ImTextCharFromUtf8(&c, beg, beg + mod.name.size());
        letter.assign(beg, size_t(n > 0 ? n : 1));
        if (letter.size() == 1) letter[0] = char(toupper(uint8_t(letter[0])));
    }
    fonts::note(letter);
    ImFont* f  = fonts::title();
    float   fs = size * 0.55f;
    ImVec2  ts = f->CalcTextSizeA(fs, FLT_MAX, 0, letter.c_str());
    dl->AddText(f, fs, {min.x + (size - ts.x) * 0.5f, min.y + (size - ts.y) * 0.5f}, col::Text, letter.c_str());
}

struct BadgeDef {
    Badge       flag;
    char const* key;
    ImU32       fill;
    ImU32       text;
};
BadgeDef const kBadges[] = {
    {Badge::Disabled, "badge.disabled", col::Red, col::Text},
    {Badge::Library, "badge.library", col::rgb(0x6B6C70), col::Text},
    {Badge::Deprecated, "badge.deprecated", col::rgb(0x8A3A1E), col::Text},
    {Badge::Modpack, "badge.modpack", col::Purple, col::Text},
    {Badge::LeviLamina, "badge.levilamina", col::Blue, col::Text},
    {Badge::Preload, "badge.preload", col::rgb(0x1E8A8A), col::Text},
    {Badge::Client, "badge.client", col::rgb(0x58585A), col::Text},
};

// Draws badges in a row starting at pos, wrapping at maxX. Returns height used.
float drawBadges(ImDrawList* dl, ModInfo const& mod, ImVec2 pos, float maxX, bool withUpdate) {
    float x = pos.x, y = pos.y, rowH = 0;
    auto put = [&](char const* label, ImU32 fill, ImU32 text) {
        float w = ore::badgeWidth(label);
        if (x + w > maxX && x > pos.x) {
            x = pos.x;
            y += rowH + s(4);
        }
        ore::badge(dl, {x, y}, label, fill, text);
        rowH = ImGui::GetFontSize() * 0.88f + s(4);
        x += w + s(4);
    };
    if (withUpdate && UpdateChecker::instance().result(mod.name).state == UpdateResult::State::Available)
        put(tr("badge.update"), col::Green, col::Text);
    for (auto const& b : kBadges) {
        if (has(mod.badges, b.flag)) put(tr(b.key), b.fill, b.text);
    }
    if (mod.enabled && !mod.loaded && !mod.entry.empty() && !mod.self)
        put(tr("badge.not_loaded"), col::rgb(0x8C8D90), col::TextDark);
    return rowH > 0 ? (y - pos.y) + rowH : 0;
}

bool hasConfig(ModInfo const& mod) {
    return ConfigRegistry::instance().has(mod.name) || !mod.configFiles.empty();
}

// ---------------------------------------------------------------- opening a mod's own menu
bool opensOwnMenu(ModInfo const& mod) { return mod.adapter && mod.adapter->open.present; }

// The player's current binding, read fresh each time so rebinding in the mod is followed.
// Returns nullopt when the key is unbound; `text` gets the raw value for messages.
std::optional<Chord> currentOpenChord(ModInfo const& mod, std::string& text) {
    auto const& open = mod.adapter->open;
    text             = open.hotkey;
    if (!open.key.empty()) {
        JsonConfigDoc doc;
        auto file = open.file.empty() ? (mod.configFiles.empty() ? std::filesystem::path() : mod.configFiles.front())
                                      : mod.folder / fromUtf8(open.file);
        if (!file.empty() && doc.load(file)) {
            if (auto* node = resolvePath(doc.data(), open.key)) {
                if (node->is_null()) return std::nullopt; // the player cleared the binding
                if (node->is_string()) text = node->get<std::string>();
            }
        }
    }
    return parseChord(text);
}

void openOwnMenu(ModInfo const& mod) {
    std::string text;
    auto        chord = currentOpenChord(mod, text);
    if (!chord) {
        showToast(text.empty() ? fmt(tr("open.unbound"), mod.name.c_str())
                               : fmt(tr("open.unknown_key"), mod.name.c_str(), text.c_str()));
        return;
    }
    int  delay  = mod.adapter->open.delayMs;
    HWND window = overlay::gameWindow();
    log::info("opening {}'s menu with {}", mod.name, describeChord(*chord));
    requestClose("open the mod's own menu");
    std::thread([c = *chord, delay, window] {
        // Wait until the menu is closed and no longer swallows input.
        for (int i = 0; i < 100 && isOpen(); ++i) Sleep(10);
        Sleep(DWORD(delay));
        sendChord(c, window);
    }).detach();
}

std::string joined(std::vector<std::string> const& v) {
    std::string out;
    for (auto const& e : v) {
        if (!out.empty()) out += ", ";
        out += e;
    }
    return out;
}

// ---------------------------------------------------------------- lip catalogue
std::optional<LipEntry> lipEntry(ModInfo const& mod) { return LipCatalog::instance().find(mod.name, mod.tooth); }

std::filesystem::path iconFor(ModInfo const& mod) {
    if (!mod.iconPath.empty()) return mod.iconPath;
    if (auto e = lipEntry(mod)) return LipCatalog::instance().icon(*e);
    return {};
}

std::string descriptionFor(ModInfo const& mod) {
    if (!mod.description.empty()) return mod.description;
    if (auto e = lipEntry(mod)) {
        if (!e->description.empty()) return e->description;
        if (auto d = LipCatalog::instance().detail(e->tooth)) return d->description;
    }
    return {};
}

// README in the UI language: zh_TW prefers Traditional, then any Chinese, then English.
LipReadme const* pickReadme(LipDetail const& d) {
    if (d.readmes.empty()) return nullptr;
    std::vector<char const*> prefs;
    if (activeLanguage() == "zh_TW") prefs = {"zh-hant", "zh-tw", "zh-hk", "zh", "en"};
    else prefs = {"en"};
    for (auto const* p : prefs) {
        for (auto const& r : d.readmes) {
            if (toLower(r.lang).rfind(p, 0) == 0) return &r;
        }
    }
    return &d.readmes.front();
}

// Parsed READMEs. std::regex recurses deeply on long inputs, so parsing runs
// on a thread with a large stack instead of the render thread.
struct ReadmeJob {
    std::string key, text;
};
std::mutex                                                       gReadmeMutex;
std::map<std::string, std::shared_ptr<markdown::Document const>> gReadmeDocs;
std::set<std::string>                                            gReadmePending;

DWORD WINAPI parseReadmeThread(LPVOID param) {
    std::unique_ptr<ReadmeJob>                job(static_cast<ReadmeJob*>(param));
    std::shared_ptr<markdown::Document const> doc;
    try {
        doc = markdown::parse(job->text);
    } catch (std::exception const& e) {
        log::warn("README parse failed ({}), showing plain text", e.what());
        auto plain = std::make_shared<markdown::Document>();
        plain->blocks.push_back({markdown::Block::Kind::Code, 0, job->text, {}});
        doc = plain;
    }
    std::lock_guard lock(gReadmeMutex);
    gReadmeDocs[job->key] = doc;
    gReadmePending.erase(job->key);
    return 0;
}

std::shared_ptr<markdown::Document const> readmeDoc(std::string const& key, std::string const& text) {
    {
        std::lock_guard lock(gReadmeMutex);
        if (auto it = gReadmeDocs.find(key); it != gReadmeDocs.end()) return it->second;
        if (gReadmePending.count(key)) return nullptr;
        gReadmePending.insert(key);
    }
    auto* job = new ReadmeJob{key, text};
    if (HANDLE t = CreateThread(nullptr, 64 * 1024 * 1024, parseReadmeThread, job, STACK_SIZE_PARAM_IS_A_RESERVATION,
                                nullptr)) {
        CloseHandle(t);
    } else {
        delete job;
        std::lock_guard lock(gReadmeMutex);
        gReadmePending.erase(key);
    }
    return nullptr;
}

void drawReadme(ModInfo const& mod) {
    auto entry = lipEntry(mod);
    if (!entry) return;
    ImGui::Dummy({0, s(6)});
    ore::text("README", col::TextDim, fonts::bold());
    auto detail = LipCatalog::instance().detail(entry->tooth);
    if (!detail) {
        ore::text(LipCatalog::instance().detailFailed(entry->tooth) ? tr("readme.failed") : tr("readme.loading"),
                  col::TextDim);
        return;
    }
    LipReadme const* readme = pickReadme(*detail);
    if (!readme) {
        ore::text(tr("readme.none"), col::TextDim);
        return;
    }
    auto doc = readmeDoc(entry->tooth + "/" + readme->file, readme->text);
    if (!doc) {
        ore::text(tr("readme.loading"), col::TextDim);
        return;
    }
    // A child with its own draw list: markdown code blocks split draw channels,
    // which cannot nest inside another split.
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(col::rgb(0x262627)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {s(12), s(10)});
    ImGui::BeginChild("readme", {0, 0}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    markdown::render(*doc);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::Dummy({0, s(6)});
}

// ---------------------------------------------------------------- list
std::vector<std::shared_ptr<ModInfo const>> visibleMods(ModList const& all) {
    std::string const query = gState.search;
    bool const showLibs = settings().showLibraries || !query.empty();
    std::vector<std::shared_ptr<ModInfo const>> out;
    for (auto const& m : all) {
        if (!showLibs && has(m->badges, Badge::Library) && !m->self) continue;
        if (!query.empty()) {
            bool match = containsIgnoreCase(m->name, query) || containsIgnoreCase(m->description, query)
                      || containsIgnoreCase(joined(m->authors), query);
            if (!match) continue;
        }
        out.push_back(m);
    }
    auto mode = SortMode(settings().sortMode.load());
    std::stable_sort(out.begin(), out.end(), [mode](auto const& a, auto const& b) {
        if (mode == SortMode::LoadedFirst && a->loaded != b->loaded) return a->loaded;
        auto la = toLower(a->name), lb = toLower(b->name);
        return mode == SortMode::NameDesc ? la > lb : la < lb;
    });
    // Children directly under their parent (parent must be visible too).
    std::vector<std::shared_ptr<ModInfo const>> ordered;
    std::set<std::string> names;
    for (auto const& m : out) names.insert(m->name);
    for (auto const& m : out) {
        if (!m->parent.empty() && names.count(m->parent)) continue;
        ordered.push_back(m);
        for (auto const& c : out) {
            if (c->parent == m->name) ordered.push_back(c);
        }
    }
    return ordered;
}

void drawModCard(ModInfo const& mod, float width, bool child) {
    ImDrawList* dl     = ImGui::GetWindowDrawList();
    float       height = s(66);
    float       indent = child ? s(22) : 0;
    ImVec2      pos    = ImGui::GetCursorScreenPos();
    ImGui::PushID(mod.name.c_str());
    bool clicked = ImGui::InvisibleButton("card", {width, height});
    bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    if (clicked) gState.selected = mod.name;
    bool selected = gState.selected == mod.name;

    ImVec2 min{pos.x + indent, pos.y}, max{pos.x + width, pos.y + height};
    ore::panel(dl, min, max, selected ? col::PanelLight : (hovered ? col::rgb(0x3B3C3E) : col::Panel), true);
    if (selected) dl->AddRect(min, max, col::Selection, 0, 0, s(2));
    if (child) dl->AddRectFilled({pos.x + s(8), pos.y}, {pos.x + s(10), pos.y + height * 0.5f}, col::PanelEdge);

    float  iconSize = s(46);
    ImVec2 iconPos{min.x + s(10), min.y + (height - iconSize) * 0.5f};
    drawIcon(dl, mod, iconPos, iconSize);

    float  tx = iconPos.x + iconSize + s(10);
    ImFont* bold = fonts::bold();
    fonts::note(mod.name);
    ImU32 nameColor = mod.enabled ? col::Text : col::TextDim;
    dl->PushClipRect(min, {max.x - s(6), max.y}, true);
    dl->AddText(bold, bold->FontSize, {tx, min.y + s(9)}, nameColor, mod.name.c_str());
    float nameW = bold->CalcTextSizeA(bold->FontSize, FLT_MAX, 0, mod.name.c_str()).x;
    if (!mod.version.empty()) {
        fonts::note(mod.version);
        dl->AddText(fonts::body(), fonts::body()->FontSize * 0.9f, {tx + nameW + s(8), min.y + s(10)}, col::TextDim,
                    mod.version.c_str());
    }
    // Second line: badges, else the first line of the description.
    float bh = drawBadges(dl, mod, {tx, min.y + s(36)}, max.x - s(8), true);
    std::string desc = descriptionFor(mod);
    if (bh == 0 && !desc.empty()) {
        std::string line = desc.substr(0, desc.find('\n'));
        fonts::note(line);
        dl->AddText(fonts::body(), fonts::body()->FontSize * 0.9f, {tx, min.y + s(36)}, col::TextDim, line.c_str());
    }
    dl->PopClipRect();
    ImGui::Dummy({0, s(4)});
}

// ---------------------------------------------------------------- details
void openConfigFor(ModInfo const& mod) {
    auto screen = ConfigRegistry::instance().get(mod.name);
    if (screen && screen->external && !gState.faultedScreens.count(mod.name)) {
        requestClose("external config screen");
        if (!safe::action(screen->external, screen->externalUser)) gState.faultedScreens.insert(mod.name);
        return;
    }
    if (screen && !screen->categories.empty()) {
        gState.view           = View::Config;
        gState.configMod      = mod.name;
        gState.configCategory = 0;
        gState.stringBuffers.clear();
        return;
    }
    if (!mod.configFiles.empty()) {
        gState.view      = View::Json;
        gState.configMod = mod.name;
        gState.docs.clear();
        for (auto const& f : mod.configFiles) {
            JsonConfigDoc doc;
            doc.load(f);
            gState.docs.push_back(std::move(doc));
        }
        gState.docIndex = 0;
        gState.stringBuffers.clear();
    }
}

void toggleEnabled(ModInfo const& mod) {
    std::string error;
    if (!ModRegistry::instance().setEnabled(mod.name, !mod.enabled, error)) {
        showToast(mod.self ? tr("self_disable") : error);
    }
}

void drawDetails(ModInfo const& mod) {
    ImDrawList* dl    = ImGui::GetWindowDrawList();
    float       width = ImGui::GetContentRegionAvail().x;
    ImVec2      pos   = ImGui::GetCursorScreenPos();

    float iconSize = s(76);
    drawIcon(dl, mod, pos, iconSize);
    float tx = pos.x + iconSize + s(14);

    fonts::note(mod.name);
    ImFont* title = fonts::title();
    dl->AddText(title, title->FontSize, {tx, pos.y}, col::Text, mod.name.c_str());
    float y = pos.y + title->FontSize + s(4);
    if (!mod.version.empty()) {
        auto v = fmt(tr("version"), mod.version.c_str());
        fonts::note(v);
        dl->AddText({tx, y}, col::TextDim, v.c_str());
        y += ImGui::GetFontSize() + s(2);
    }
    if (!mod.authors.empty()) {
        auto a = fmt(tr("by"), joined(mod.authors).c_str());
        fonts::note(a);
        dl->AddText({tx, y}, col::TextDim, a.c_str());
        y += ImGui::GetFontSize() + s(2);
    }
    y += s(4);
    y += drawBadges(dl, mod, {tx, y}, pos.x + width, true);
    ImGui::SetCursorScreenPos({pos.x, std::max(pos.y + iconSize, y) + s(14)});

    // Actions
    auto screen       = ConfigRegistry::instance().get(mod.name);
    bool apiScreen    = screen && (screen->external || !screen->categories.empty());
    bool configurable = hasConfig(mod) && !gState.faultedScreens.count(mod.name);
    if (!apiScreen && opensOwnMenu(mod)) {
        // Config opens the mod's own menu; its JSON files stay one click away.
        std::string label = mod.adapter->open.label.empty() ? tr("config") : mod.adapter->open.label.get();
        if (ore::button("##config", label.c_str(), ore::Variant::Primary)) openOwnMenu(mod);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) { // reads the mod's config, so only on hover
            std::string text;
            auto        chord = currentOpenChord(mod, text);
            std::string tip   = chord ? fmt(tr("open.tip"), mod.name.c_str(), describeChord(*chord).c_str())
                                      : fmt(tr("open.unbound"), mod.name.c_str());
            ore::tooltip(tip.c_str());
        }
        if (!mod.configFiles.empty()) {
            ImGui::SameLine();
            if (ore::button("##configfiles", tr("json_editor"), ore::Variant::Dark)) openConfigFor(mod);
        }
    } else if (ore::button("##config", tr("config"), ore::Variant::Primary, {0, 0}, !configurable)) {
        openConfigFor(mod);
    }
    ImGui::SameLine();
    bool isLL = mod.name == "LeviLamina";
    if (ore::button("##toggle", mod.enabled ? tr("disable") : tr("enable"),
                    mod.enabled ? ore::Variant::Destructive : ore::Variant::Primary, {0, 0}, mod.self)) {
        if (isLL && mod.enabled) gState.confirmDisableLL = true;
        else toggleEnabled(mod);
    }
    if (mod.self) ore::tooltip(tr("self_disable"));
    ImGui::SameLine();
    if (ore::button("##folder", tr("open_folder"), ore::Variant::Dark)) openFolder(mod.folder);

    // Update box
    auto upd = UpdateChecker::instance().result(mod.name);
    std::string updText;
    ImU32       updFill = col::Inset;
    switch (upd.state) {
    case UpdateResult::State::Available:
        updText = fmt(tr("update.available"), upd.latest.c_str());
        updFill = col::GreenLip;
        break;
    case UpdateResult::State::NeedsNewerGame: updText = fmt(tr("update.newer_game"), upd.newestAny.c_str()); break;
    case UpdateResult::State::Checking: updText = tr("update.checking"); break;
    case UpdateResult::State::Error: updText = fmt(tr("update.error"), upd.error.c_str()); break;
    case UpdateResult::State::UpToDate: updText = tr("update.uptodate"); break;
    default: break;
    }
    if (!updText.empty()) {
        ImGui::Dummy({0, s(2)});
        ImVec2 bmin = ImGui::GetCursorScreenPos();
        float  h    = ImGui::GetFontSize() + s(18);
        bool   link = !upd.url.empty()
                 && (upd.state == UpdateResult::State::Available || upd.state == UpdateResult::State::NeedsNewerGame);
        ore::panel(dl, bmin, {bmin.x + width, bmin.y + h}, updFill, false);
        fonts::note(updText);
        dl->AddText({bmin.x + s(10), bmin.y + s(9)}, col::Text, updText.c_str());
        ImGui::Dummy({width, h});
        if (link) {
            if (ore::button("##upd", tr("update.open"), ore::Variant::Dark)) openExternal(upd.url);
        }
    }

    ImGui::Dummy({0, s(6)});

    // Manifest data first, the lip catalogue fills the gaps.
    auto                               entry   = lipEntry(mod);
    auto                               detail  = entry ? LipCatalog::instance().detail(entry->tooth) : nullptr;
    std::map<std::string, std::string> links   = mod.links;
    std::string                        license = mod.license;
    if (entry) {
        if (license.empty()) license = entry->license;
        if (!entry->page.empty()) links.try_emplace("levimc", entry->page);
    }
    if (detail) {
        if (license.empty()) license = detail->license;
        if (!detail->repository.empty()) links.try_emplace("source", detail->repository);
        if (!detail->issues.empty()) links.try_emplace("issues", detail->issues);
    }

    // Links
    if (!links.empty()) {
        ore::text(tr("links"), col::TextDim, fonts::bold());
        bool first = true;
        for (auto const& [key, url] : links) {
            std::string labelKey = "link." + key;
            char const* label    = tr(labelKey.c_str());
            if (label == labelKey.c_str() || std::string(label) == labelKey) label = key.c_str();
            float w = ore::badgeWidth(label) + s(24);
            if (!first && ImGui::GetCursorPosX() + w < ImGui::GetContentRegionMax().x) ImGui::SameLine();
            first = false;
            ImGui::PushID(key.c_str());
            if (ore::button("##link", label, ore::Variant::Dark)) openExternal(url);
            ore::tooltip(url.c_str());
            ImGui::PopID();
        }
        ImGui::Dummy({0, s(6)});
    }

    // Technical details
    ore::text(tr("details"), col::TextDim, fonts::bold());
    if (ImGui::BeginTable("details", 2, ImGuiTableFlags_SizingStretchProp)) {
        auto row = [](char const* k, std::string const& v) {
            if (v.empty()) return;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ore::text(k, col::TextDim);
            ImGui::TableNextColumn();
            ore::textWrapped(v.c_str());
        };
        std::string state = !mod.enabled ? tr("state.disabled")
                          : (mod.loaded || mod.self) ? tr("state.loaded")
                                                     : tr("state.not_loaded");
        row(tr("state"), state);
        row(tr("license"), license);
        row(tr("contributors"), joined(mod.contributors));
        row(tr("type"), mod.type);
        row(tr("entry"), mod.entry);
        row(tr("folder"), pathUtf8(mod.folder));
        ImGui::EndTable();
    }

    // Description, then the README from the lip catalogue (GitHub).
    if (std::string desc = descriptionFor(mod); !desc.empty()) {
        ImGui::Dummy({0, s(8)});
        ore::text(tr("description"), col::TextDim, fonts::bold());
        ore::textWrapped(desc.c_str(), col::Text);
    }
    drawReadme(mod);
}

// ---------------------------------------------------------------- option cards
// Settings are laid out as OreUI rows: a card per option with the label (and
// an optional description) on the left and the control on the right, inside
// a centred column so wide windows do not stretch them apart.
struct Column {
    float x0 = 0, width = 0;
};

Column settingsColumn() {
    float avail = ImGui::GetContentRegionAvail().x;
    float w     = std::min(avail, s(940));
    return {ImGui::GetCursorPosX() + (avail - w) * 0.5f, w};
}

void sectionHeader(Column const& c, std::string const& title, std::string const& description = {}) {
    ImGui::Dummy({0, s(6)});
    ImGui::SetCursorPosX(c.x0);
    fonts::note(title);
    ImGui::PushFont(fonts::bold());
    ImGui::TextUnformatted(title.c_str());
    ImGui::PopFont();
    ImVec2 p = ImGui::GetCursorScreenPos();
    p.x      = ImGui::GetWindowPos().x + c.x0 - ImGui::GetScrollX();
    ImGui::GetWindowDrawList()->AddRectFilled({p.x, p.y}, {p.x + c.width, p.y + s(2)}, col::PanelEdge);
    ImGui::Dummy({0, s(4)});
    if (!description.empty()) {
        ImGui::SetCursorPosX(c.x0);
        ImGui::PushTextWrapPos(c.x0 + c.width);
        ore::text(description.c_str(), col::TextDim);
        ImGui::PopTextWrapPos();
    }
}

enum class ControlKind { Field, Toggle, Button };

// Draws one option card. `control` draws the widget; it gets the width to use.
template <class F>
void optionCard(Column const& c, char const* id, std::string const& label, std::string const& description,
                std::string const& tooltip, ControlKind kind, F&& control) {
    ImGui::PushID(id);
    ImDrawList* dl      = ImGui::GetWindowDrawList();
    float       pad     = s(10);
    float       ctrlW   = kind == ControlKind::Toggle ? s(58) : std::min(s(320), c.width * 0.45f);
    ImVec2      winPos  = ImGui::GetWindowPos();
    float       scrollY = ImGui::GetScrollY();
    float       startY  = ImGui::GetCursorPosY();
    ImVec2      start{winPos.x + c.x0 - ImGui::GetScrollX(), winPos.y + startY - scrollY};

    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    // Label + description
    // Single-line labels sit centred on the 30px control.
    ImGui::SetCursorPos({c.x0 + pad, startY + pad + s(6)});
    ImGui::BeginGroup();
    float textRight = c.x0 + c.width - ctrlW - pad * 2;
    ImGui::PushTextWrapPos(textRight);
    fonts::note(label);
    ImGui::TextUnformatted(label.c_str());
    if (!tooltip.empty()) ore::tooltip(tooltip.c_str());
    if (!description.empty()) {
        fonts::note(description);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(col::TextDim));
        ImGui::TextUnformatted(description.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    float textBottom = ImGui::GetItemRectMax().y;
    // Control, top-aligned with the label
    float ctrlX = c.x0 + c.width - pad - ctrlW;
    ImGui::SetCursorPos({ctrlX, startY + pad});
    ImGui::BeginGroup();
    ImGui::SetNextItemWidth(ctrlW);
    control(ctrlW);
    ImGui::EndGroup();
    float bottom = std::max(textBottom + s(6), ImGui::GetItemRectMax().y) + pad;

    dl->ChannelsSetCurrent(0);
    ore::panel(dl, start, {start.x + c.width, bottom}, col::rgb(0x3A3B3C), false);
    dl->ChannelsMerge();
    ImGui::SetCursorScreenPos({start.x, bottom + s(4)});
    ImGui::Dummy({c.width, 0});
    ImGui::PopID();
}

bool inputStdString(char const* id, std::string& value) {
    auto&   buf = gState.stringBuffers[std::to_string(ImGui::GetID(id))];
    size_t  cap = std::max<size_t>(256, value.size() * 2 + 64);
    ImGuiID gid = ImGui::GetID(id);
    if (ImGui::GetActiveID() != gid || buf.size() < value.size() + 1) {
        buf.assign(cap, 0);
        memcpy(buf.data(), value.data(), value.size());
    }
    fonts::note(value);
    if (ImGui::InputText(id, buf.data(), buf.size())) {
        value = buf.data();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------- config (API screens)
bool drawOption(Column const& c, ConfigOption const& o) {
    bool ok = true;
    char const* id = o.id.empty() ? o.label.c_str() : o.id.c_str();
    switch (o.kind) {
    case OptionKind::Header: sectionHeader(c, o.label, o.tooltip); break;
    case OptionKind::Bool: {
        bool v = false;
        ok     = safe::getBool(o, v);
        if (ok) optionCard(c, id, o.label, o.tooltip, {}, ControlKind::Toggle, [&](float) {
            if (ore::toggle("##v", &v)) ok = safe::setBool(o, v);
        });
        break;
    }
    case OptionKind::Int: {
        int64_t v = 0;
        ok        = safe::getInt(o, v);
        if (ok) optionCard(c, id, o.label, o.tooltip, {}, ControlKind::Field, [&](float) {
            bool changed = o.imax - o.imin <= 100000 ? ImGui::SliderScalar("##v", ImGuiDataType_S64, &v, &o.imin, &o.imax)
                                                     : ImGui::InputScalar("##v", ImGuiDataType_S64, &v);
            if (changed) ok = safe::setInt(o, std::clamp(v, o.imin, o.imax));
        });
        break;
    }
    case OptionKind::Float: {
        double v = 0;
        ok       = safe::getFloat(o, v);
        if (ok) optionCard(c, id, o.label, o.tooltip, {}, ControlKind::Field, [&](float) {
            if (ImGui::SliderScalar("##v", ImGuiDataType_Double, &v, &o.fmin, &o.fmax, o.format.c_str())) {
                if (o.step > 0) v = o.fmin + std::round((v - o.fmin) / o.step) * o.step;
                ok = safe::setFloat(o, std::clamp(v, o.fmin, o.fmax));
            }
        });
        break;
    }
    case OptionKind::String: {
        auto& buf = gState.stringBuffers[o.id];
        buf.resize(o.maxBytes);
        optionCard(c, id, o.label, o.tooltip, {}, ControlKind::Field, [&](float) {
            if (ImGui::GetActiveID() != ImGui::GetID("##v")) ok = safe::getString(o, buf.data(), uint32_t(buf.size()));
            fonts::note(buf.data());
            if (ok && ImGui::InputText("##v", buf.data(), buf.size())) ok = safe::setString(o, buf.data());
        });
        break;
    }
    case OptionKind::Enum: {
        int64_t v = 0;
        ok        = safe::getInt(o, v);
        if (ok) optionCard(c, id, o.label, o.tooltip, {}, ControlKind::Field, [&](float) {
            char const* preview = v >= 0 && size_t(v) < o.choices.size() ? o.choices[size_t(v)].c_str() : "?";
            for (auto const& ch : o.choices) fonts::note(ch);
            if (ImGui::BeginCombo("##v", preview)) {
                for (size_t i = 0; i < o.choices.size(); ++i) {
                    if (ImGui::Selectable(o.choices[i].c_str(), int64_t(i) == v)) ok = safe::setInt(o, int64_t(i));
                }
                ImGui::EndCombo();
            }
        });
        break;
    }
    case OptionKind::Button:
        optionCard(c, id, o.label, o.tooltip, {}, ControlKind::Button, [&](float w) {
            if (ore::button("##b", o.label.c_str(), ore::Variant::Dark, {w, 0})) ok = safe::action(o.onClick, o.user);
        });
        break;
    }
    return ok;
}

void finishConfig() {
    if (gState.view == View::Config) {
        if (auto screen = ConfigRegistry::instance().get(gState.configMod)) {
            if (!safe::action(screen->onSave, screen->onSaveUser)) gState.faultedScreens.insert(gState.configMod);
        }
    }
    gState.view = View::List;
    gState.docs.clear();
    gState.stringBuffers.clear();
}

// Vertical tab list used by both editors when there is more than one page.
void tabList(float height, std::vector<std::string> const& names, int& current, float width) {
    ImGui::BeginChild("tabs", {width, height});
    for (int i = 0; i < int(names.size()); ++i) {
        ImGui::PushID(i);
        if (ore::button("##tab", names[size_t(i)].c_str(), i == current ? ore::Variant::Secondary : ore::Variant::Dark,
                        {width - s(4), 0}))
            current = i;
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();
}

void drawConfigBody(float height) {
    auto screen = ConfigRegistry::instance().get(gState.configMod);
    if (!screen || screen->categories.empty() || gState.faultedScreens.count(gState.configMod)) {
        ore::text(tr("no_config"), col::TextDim);
        return;
    }
    int& cat = gState.configCategory;
    cat      = std::clamp(cat, 0, int(screen->categories.size()) - 1);
    if (screen->categories.size() > 1) {
        std::vector<std::string> names;
        for (auto const& c : screen->categories) names.push_back(c.title.empty() ? tr("general") : c.title);
        tabList(height, names, cat, s(210));
    }
    ImGui::BeginChild("options", {0, height});
    Column column = settingsColumn();
    for (auto const& o : screen->categories[size_t(cat)].options) {
        if (!drawOption(column, o)) {
            log::error("config screen of {} faulted in option '{}'; disabling it", gState.configMod, o.id);
            gState.faultedScreens.insert(gState.configMod);
            break;
        }
    }
    ImGui::Dummy({0, s(8)});
    ImGui::EndChild();
}

// ---------------------------------------------------------------- JSON config editor
using ojson = nlohmann::ordered_json;

std::string choiceLabel(AdapterOption const* opt, ojson const& value) {
    if (opt) {
        for (auto const& ch : opt->choices)
            if (ch.value == value) return ch.label.get();
    }
    return value.is_string() ? value.get<std::string>() : value.dump();
}

// One value as a card. `opt` adds labels, ranges and choices from the adapter.
void jsonValueCard(Column const& c, ojson& node, std::string const& path, AdapterOption const* opt) {
    std::string label = opt && !opt->label.empty() ? opt->label.get() : humanizeKey(path);
    std::string desc  = opt ? opt->description.get() : std::string();
    std::string tip   = path; // the raw key, for people who edit the file by hand
    bool        ro    = opt && opt->readOnly;

    if (opt && !opt->choices.empty()) {
        optionCard(c, path.c_str(), label, desc, tip, ControlKind::Field, [&](float) {
            std::string preview = choiceLabel(opt, node);
            fonts::note(preview);
            ImGui::BeginDisabled(ro);
            if (ImGui::BeginCombo("##v", preview.c_str())) {
                for (auto const& ch : opt->choices) {
                    std::string l = ch.label.get();
                    fonts::note(l);
                    if (ImGui::Selectable(l.c_str(), ch.value == node)) node = ch.value;
                }
                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
        });
        return;
    }
    switch (node.type()) {
    case ojson::value_t::boolean:
        optionCard(c, path.c_str(), label, desc, tip, ControlKind::Toggle, [&](float) {
            bool v = node.get<bool>();
            if (ore::toggle("##v", &v, ro)) node = v;
        });
        break;
    case ojson::value_t::number_integer:
    case ojson::value_t::number_unsigned:
        optionCard(c, path.c_str(), label, desc, tip, ControlKind::Field, [&](float) {
            int64_t v = node.is_number_unsigned() ? int64_t(node.get<uint64_t>()) : node.get<int64_t>();
            ImGui::BeginDisabled(ro);
            bool changed;
            if (opt && opt->min && opt->max) {
                int64_t lo = int64_t(*opt->min), hi = int64_t(*opt->max);
                changed = ImGui::SliderScalar("##v", ImGuiDataType_S64, &v, &lo, &hi,
                                              opt->format.empty() ? "%lld" : opt->format.c_str());
                if (opt->step && *opt->step > 1) v = lo + int64_t(std::llround(double(v - lo) / *opt->step) * *opt->step);
                v = std::clamp(v, lo, hi);
            } else {
                changed = ImGui::InputScalar("##v", ImGuiDataType_S64, &v);
            }
            if (changed) node = v;
            ImGui::EndDisabled();
        });
        break;
    case ojson::value_t::number_float:
        optionCard(c, path.c_str(), label, desc, tip, ControlKind::Field, [&](float) {
            double v = node.get<double>();
            ImGui::BeginDisabled(ro);
            bool changed;
            if (opt && opt->min && opt->max) {
                double lo = *opt->min, hi = *opt->max;
                changed = ImGui::SliderScalar("##v", ImGuiDataType_Double, &v, &lo, &hi,
                                              opt->format.empty() ? "%.2f" : opt->format.c_str());
                if (opt->step && *opt->step > 0) v = lo + std::round((v - lo) / *opt->step) * *opt->step;
                v = std::clamp(v, lo, hi);
            } else {
                changed = ImGui::InputDouble("##v", &v, 0, 0, "%.6g");
            }
            if (changed) node = v;
            ImGui::EndDisabled();
        });
        break;
    case ojson::value_t::string:
        optionCard(c, path.c_str(), label, desc, tip, ControlKind::Field, [&](float) {
            std::string v = node.get<std::string>();
            ImGui::BeginDisabled(ro);
            if (inputStdString("##v", v)) node = v;
            ImGui::EndDisabled();
        });
        break;
    case ojson::value_t::array:
        optionCard(c, path.c_str(), label, desc, tip, ControlKind::Field, [&](float) {
            ore::text(fmt(tr("array_items"), int(node.size())).c_str(), col::TextDim);
        });
        break;
    default:
        optionCard(c, path.c_str(), label, desc, tip, ControlKind::Field, [&](float) { ore::text("null", col::TextDim); });
        break;
    }
}

bool isHidden(AdapterFile const* file, std::string const& path) {
    if (!file) return false;
    for (auto const& h : file->hidden) {
        if (path == h || (path.size() > h.size() && path.compare(0, h.size(), h) == 0 && path[h.size()] == '.'))
            return true;
    }
    return false;
}

// Everything under `node` that is not hidden or already shown: primitives of
// one object form a section titled by its path ("Hud › Projection › Show").
void genericSections(Column const& c, ojson& node, std::string const& path, AdapterFile const* file,
                     std::set<std::string> const& shown, bool& headerDrawn, std::string const& fallbackTitle,
                     std::string const& noHeaderPath = "") {
    if (!node.is_object()) return;
    // An object an adapter section points at already has that section's title.
    bool sectionStarted = path == noHeaderPath;
    for (auto& [k, v] : node.items()) {
        std::string child = path.empty() ? k : path + "." + k;
        if (v.is_object() || isHidden(file, child) || shown.count(child)) continue;
        if (!sectionStarted) {
            std::string title;
            if (path.empty()) title = fallbackTitle;
            else {
                size_t pos = 0;
                std::string p = path;
                while (true) {
                    size_t dot = p.find('.', pos);
                    title += (title.empty() ? "" : " › ") + humanizeKey(p.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos));
                    if (dot == std::string::npos) break;
                    pos = dot + 1;
                }
            }
            sectionHeader(c, title);
            sectionStarted = headerDrawn = true;
        }
        jsonValueCard(c, v, child, nullptr);
    }
    for (auto& [k, v] : node.items()) {
        std::string child = path.empty() ? k : path + "." + k;
        if (v.is_object() && !isHidden(file, child) && !shown.count(child))
            genericSections(c, v, child, file, shown, headerDrawn, fallbackTitle, noHeaderPath);
    }
}

void drawJsonBody(float height) {
    if (gState.docs.empty()) {
        ore::text(tr("no_config"), col::TextDim);
        return;
    }
    auto mod = ModRegistry::instance().find(gState.configMod);
    int& idx = gState.docIndex;
    idx      = std::clamp(idx, 0, int(gState.docs.size()) - 1);

    auto fileFor = [&](JsonConfigDoc const& d) -> AdapterFile const* {
        if (!mod || !mod->adapter) return nullptr;
        return mod->adapter->fileFor(d.path().lexically_relative(mod->folder));
    };
    if (gState.docs.size() > 1) {
        std::vector<std::string> names;
        for (auto const& d : gState.docs) {
            AdapterFile const* f    = fileFor(d);
            std::string        name = f && !f->title.empty() ? f->title.get() : pathUtf8(d.path().filename());
            if (d.dirty()) name += " *";
            names.push_back(name);
        }
        tabList(height, names, idx, s(220));
    }

    ImGui::BeginChild("editor", {0, height});
    auto&              doc  = gState.docs[size_t(idx)];
    AdapterFile const* file = fileFor(doc);
    Column             c    = settingsColumn();
    if (!file || file->restartRequired) {
        ImGui::SetCursorPosX(c.x0);
        ImGui::PushTextWrapPos(c.x0 + c.width);
        ore::text(tr("json_editor_hint"), col::TextDim);
        ImGui::PopTextWrapPos();
    }
    if (!doc.loaded()) {
        ImGui::SetCursorPosX(c.x0);
        ore::textWrapped(fmt(tr("json_parse_error"), doc.error().c_str()).c_str(), col::Red);
    } else {
        std::set<std::string> shown;
        if (file) {
            for (auto const& section : file->sections) {
                bool any = false;
                for (auto const& opt : section.options) {
                    ojson* node = resolvePath(doc.data(), opt.key);
                    if (!node || isHidden(file, opt.key)) continue;
                    if (!any) {
                        sectionHeader(c, section.title.get(), section.description.get());
                        any = true;
                    }
                    shown.insert(opt.key);
                    if (node->is_object()) {
                        bool drawn = false;
                        genericSections(c, *node, opt.key, file, shown, drawn, {}, opt.key);
                    } else {
                        jsonValueCard(c, *node, opt.key, &opt);
                    }
                }
            }
        }
        if (!file || !file->listedOnly) {
            bool drawn = false;
            genericSections(c, doc.data(), {}, file, shown, drawn, file && !file->sections.empty() ? tr("other") : tr("general"));
        }
    }
    ImGui::Dummy({0, s(8)});
    ImGui::EndChild();
}

// ---------------------------------------------------------------- frame
void applyRequests() {
    Requests r;
    {
        std::lock_guard lock(gRequestMutex);
        r         = std::move(gRequests);
        gRequests = {};
        gPending  = false;
    }
    bool wasOpen = gOpen;
    bool open    = wasOpen;
    if (r.toggle) open = !open;
    if (r.open) open = true;
    if (r.close) open = false;
    if (open && !wasOpen) {
        ModRegistry::instance().rescan();
        LipCatalog::instance().loadAsync();
        if (settings().checkUpdatesOnOpen && !gState.checkedUpdatesThisSession) {
            gState.checkedUpdatesThisSession = true;
            UpdateChecker::instance().checkAll();
        }
        ClipCursor(nullptr);
        gState.view = View::List;
    } else if (r.rescan && open) {
        ModRegistry::instance().rescan();
    }
    if (!r.selectMod.empty()) {
        gState.selected = r.selectMod;
        if (r.openConfig) {
            if (auto m = ModRegistry::instance().find(r.selectMod)) openConfigFor(*m);
        }
    }
    if (!open && wasOpen) {
        if (gState.view != View::List) finishConfig();
        gCapturingHotkey = false;
    }
    gOpen = open;
}

void drawHeader(ImVec2 min, float width, float height, char const* titleText, bool showBack,
                char const* subtitle = nullptr) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ore::panel(dl, min, {min.x + width, min.y + height}, col::PanelLight, true);
    ImFont* title = fonts::title();
    fonts::note(titleText);
    float tx = min.x + s(16);
    if (showBack) {
        ImGui::SetCursorScreenPos({min.x + s(10), min.y + (height - s(38)) * 0.5f});
        if (ore::button("##back", tr("back"), ore::Variant::Dark, {0, s(38)})) finishConfig();
        tx = ImGui::GetItemRectMax().x + s(12);
    }
    dl->AddText(title, title->FontSize, {tx, min.y + (height - title->FontSize) * 0.5f}, col::Text, titleText);
    if (subtitle && *subtitle) {
        fonts::note(subtitle);
        float tw = title->CalcTextSizeA(title->FontSize, FLT_MAX, 0, titleText).x;
        dl->AddText({tx + tw + s(14), min.y + (height - ImGui::GetFontSize()) * 0.5f + s(2)}, col::TextDim, subtitle);
    }
    ImGui::SetCursorScreenPos({min.x + width - s(48), min.y + (height - s(38)) * 0.5f});
    if (ore::iconButton("##close", "X", ore::Variant::Dark, s(38))) requestClose("close button");
}

void drawListView(ImVec2 bodyMin, ImVec2 bodyMax) {
    auto all  = ModRegistry::instance().mods();
    auto mods = visibleMods(*all);
    float total = bodyMax.x - bodyMin.x;
    float leftW = std::max(s(320), total * 0.42f);
    float h     = bodyMax.y - bodyMin.y;

    // Left column: search + filters + list
    ImGui::SetCursorScreenPos(bodyMin);
    ImGui::BeginChild("left", {leftW, h});
    ore::searchField("##search", gState.search, sizeof(gState.search), tr("search"), leftW - s(4));
    fonts::note(gState.search);
    bool libs = settings().showLibraries;
    if (ore::toggle("##libs", &libs)) {
        settings().showLibraries = libs;
        settings().save();
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ore::text(tr("show_libraries"), col::TextDim);
    ImGui::SameLine();
    char const* sortKeys[] = {"sort.name_asc", "sort.name_desc", "sort.loaded"};
    int         sort       = std::clamp(settings().sortMode.load(), 0, 2);
    float       comboW     = s(150);
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), leftW - comboW - s(4)));
    ImGui::SetNextItemWidth(comboW);
    if (ImGui::BeginCombo("##sort", tr(sortKeys[sort]))) {
        for (int i = 0; i < 3; ++i) {
            if (ImGui::Selectable(tr(sortKeys[i]), i == sort)) {
                settings().sortMode = i;
                settings().save();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::BeginChild("list", {0, 0}, ImGuiChildFlags_None);
    float cardW = ImGui::GetContentRegionAvail().x;
    if (mods.empty()) ore::textWrapped(tr("no_mods"), col::TextDim);
    for (auto const& m : mods) {
        bool child = !m->parent.empty()
                  && std::any_of(mods.begin(), mods.end(), [&](auto const& p) { return p->name == m->parent; });
        drawModCard(*m, cardW, child);
    }
    ImGui::EndChild();
    ImGui::EndChild();

    // Right column: details
    ImGui::SetCursorScreenPos({bodyMin.x + leftW + s(12), bodyMin.y});
    ImVec2 dmin = ImGui::GetCursorScreenPos();
    ore::panel(ImGui::GetWindowDrawList(), dmin, bodyMax, col::rgb(0x2A2A2B), false);
    ImGui::SetCursorScreenPos({dmin.x + s(14), dmin.y + s(14)});
    ImGui::BeginChild("details", {bodyMax.x - dmin.x - s(20), h - s(20)});
    // A newly selected mod starts at the top of its details.
    static std::string shownDetails;
    if (shownDetails != gState.selected) {
        ImGui::SetScrollY(0);
        shownDetails = gState.selected;
    }
    auto sel = ModRegistry::instance().find(gState.selected);
    if (!sel && !mods.empty()) {
        gState.selected = mods.front()->name;
        sel             = mods.front();
    }
    if (sel) drawDetails(*sel);
    else ore::textWrapped(tr("no_selection"), col::TextDim);
    ImGui::EndChild();
}

void drawFooter(ImVec2 min, float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float       by = min.y + (height - s(40)) * 0.5f;
    // Right side buttons first so we know where the left text must stop.
    float x = min.x + width;
    auto  placeRight = [&](char const* id, char const* label, ore::Variant v, bool disabled) {
        fonts::note(label);
        float w = fonts::bold()->CalcTextSizeA(fonts::bold()->FontSize, FLT_MAX, 0, label).x + s(32);
        x -= w;
        ImGui::SetCursorScreenPos({x, by});
        bool r = ore::button(id, label, v, {w, s(40)}, disabled);
        x -= s(8);
        return r;
    };

    if (gState.view == View::List) {
        if (placeRight("##modsdir", tr("open_mods_folder"), ore::Variant::Dark, false)) openFolder(paths::modsDir());
        if (placeRight("##refresh", tr("refresh"), ore::Variant::Dark, false)) ModRegistry::instance().rescan();
        bool running = UpdateChecker::instance().running();
        if (placeRight("##updates", running ? tr("checking_updates") : tr("check_updates"), ore::Variant::Dark,
                       running))
            UpdateChecker::instance().checkAll();
    } else if (gState.view == View::Config) {
        if (placeRight("##done", tr("done"), ore::Variant::Primary, false)) finishConfig();
    } else if (gState.view == View::Json && !gState.docs.empty()) {
        auto& doc = gState.docs[size_t(std::clamp(gState.docIndex, 0, int(gState.docs.size()) - 1))];
        if (placeRight("##save", tr("save"), ore::Variant::Primary, !doc.dirty())) {
            if (doc.save()) showToast(tr("saved"));
            else showToast(doc.error());
        }
        if (placeRight("##revert", tr("revert"), ore::Variant::Dark, !doc.dirty())) doc.revert();
    }

    // Left: toast > restart notice > hint
    std::string msg;
    ImU32       color = col::TextDim;
    if (!gState.toast.empty() && ImGui::GetTime() < gState.toastUntil) {
        msg   = gState.toast;
        color = col::Yellow;
    } else if (ModRegistry::instance().needsRestart()) {
        msg   = tr("restart_required");
        color = col::Yellow;
    } else if (gState.view == View::List && !ModRegistry::instance().leviLaminaLoaded()) {
        msg = tr("leviLamina_missing");
    } else {
        msg = fmt(tr("hint"), keyName(settings().hotkey).c_str());
    }
    fonts::note(msg);
    dl->PushClipRect(min, {x, min.y + height}, true);
    dl->AddText({min.x + s(4), min.y + (height - ImGui::GetFontSize()) * 0.5f}, color, msg.c_str());
    dl->PopClipRect();
}

void drawConfirm() {
    if (gState.confirmDisableLL) {
        ImGui::OpenPopup("##confirm");
        gState.confirmDisableLL = false;
    }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({s(460), 0});
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImGui::ColorConvertU32ToFloat4(col::PanelLight));
    if (ImGui::BeginPopupModal("##confirm", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize)) {
        ore::textWrapped(tr("confirm_disable_ll"));
        ImGui::Dummy({0, s(8)});
        if (ore::button("##yes", tr("disable"), ore::Variant::Destructive)) {
            if (auto m = ModRegistry::instance().find("LeviLamina")) toggleEnabled(*m);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ore::button("##no", tr("cancel"), ore::Variant::Secondary) || ImGui::IsKeyPressed(ImGuiKey_Escape))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

} // namespace

// ---------------------------------------------------------------- MenuControl
void requestOpen(std::string mod, bool openConfig) {
    std::lock_guard lock(gRequestMutex);
    gRequests.open       = true;
    gRequests.close      = false;
    gRequests.selectMod  = std::move(mod);
    gRequests.openConfig = openConfig;
    gPending             = true;
}
void requestClose(char const* reason) {
    log::info("menu close requested ({})", reason);
    std::lock_guard lock(gRequestMutex);
    gRequests.close = true;
    gRequests.open  = false;
    gPending        = true;
}
void requestToggle(char const* reason) {
    log::info("menu toggle requested ({})", reason);
    std::lock_guard lock(gRequestMutex);
    gRequests.toggle = !gRequests.toggle;
    gPending         = true;
}
void requestRescan() {
    std::lock_guard lock(gRequestMutex);
    gRequests.rescan = true;
    gPending         = true;
}
bool isOpen() { return gOpen; }

// ---------------------------------------------------------------- Ui.h
bool wantsFrame() { return gOpen || gPending; }

bool prepareFrame(float width, float height) {
    applyRequests();
    icons::pump();
    // Design size is 1080p; follow the window and the user's scale.
    float scale = std::clamp(height / 960.0f, 0.6f, 3.0f) * float(settings().uiScale.load());
    scale       = std::round(scale * 20.0f) / 20.0f;
    // While the mouse is held (e.g. dragging the UI scale slider) keep the old
    // scale: rescaling resizes the slider under the cursor, which feeds back
    // into its value, and every step would rebuild the font atlas.
    if (gState.appliedScale > 0 && ImGui::GetIO().MouseDown[0]) scale = gState.appliedScale;
    bool rebuilt = fonts::update(scale);
    if (rebuilt || std::abs(gState.appliedScale - scale) > 0.001f) {
        ore::applyStyle(scale);
        gState.appliedScale = scale;
    }
    (void)width;
    return rebuilt;
}

void drawFrame() {
    if (!gOpen) return;
    ImGuiIO& io = ImGui::GetIO();
    // Software cursor when the game has hidden the OS one (in-world).
    CURSORINFO ci{sizeof(ci)};
    io.MouseDrawCursor = GetCursorInfo(&ci) && !(ci.flags & CURSOR_SHOWING);

    // Esc: leave the sub view, else close. Not while a text field or popup has focus.
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && ImGui::GetActiveID() == 0 && !io.WantTextInput
        && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
        if (gState.view != View::List) finishConfig();
        else requestClose("Esc");
    }

    ImVec2 display = io.DisplaySize;
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::Begin("##modmenu", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                     | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus
                     | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled({0, 0}, display, col::Dim);

    bool  full   = settings().fullscreen;
    float margin = full ? 0.0f : std::max(s(24), display.y * 0.04f);
    float width  = full ? display.x : std::min(display.x - margin * 2, s(1280));
    float height = display.y - margin * 2;
    ImVec2 min{(display.x - width) * 0.5f, margin};
    ImVec2 max{min.x + width, min.y + height};
    ore::panel(dl, min, max, col::Panel, true);

    float headerH = s(60), footerH = s(58), pad = s(14);
    std::string titleText, subtitle;
    if (gState.view == View::List) {
        auto all    = ModRegistry::instance().mods();
        int  loaded = 0;
        for (auto const& m : *all) loaded += (m->loaded || m->self) ? 1 : 0;
        titleText = tr("title");
        subtitle  = fmt(tr("count"), int(all->size()), loaded);
        if (int n = UpdateChecker::instance().availableCount()) subtitle += "  ·  " + fmt(tr("updates_count"), n);
    } else {
        auto screen = ConfigRegistry::instance().get(gState.configMod);
        titleText   = screen && !screen->title.empty() ? screen->title
                                                       : gState.configMod + " · "
                                                             + (gState.view == View::Json ? tr("json_editor") : tr("config"));
    }
    drawHeader(min, width, headerH, titleText.c_str(), gState.view != View::List, subtitle.c_str());

    ImVec2 bodyMin{min.x + pad, min.y + headerH + pad};
    ImVec2 bodyMax{max.x - pad, max.y - footerH};
    if (gState.view == View::List) {
        drawListView(bodyMin, bodyMax);
    } else {
        ImGui::SetCursorScreenPos(bodyMin);
        ImGui::BeginChild("sub", {bodyMax.x - bodyMin.x, bodyMax.y - bodyMin.y});
        float h = bodyMax.y - bodyMin.y;
        if (gState.view == View::Config) drawConfigBody(h);
        else drawJsonBody(h);
        ImGui::EndChild();
    }

    dl->AddRectFilled({min.x + s(2), max.y - footerH}, {max.x - s(2), max.y - footerH + s(2)}, col::Outline);
    drawFooter({min.x + pad, max.y - footerH + s(2)}, width - pad * 2, footerH - s(2));
    drawConfirm();
    ImGui::End();
}

void onDeviceLost() { icons::clear(); }

bool handleWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg != WM_KEYDOWN && msg != WM_SYSKEYDOWN) return false;
    if (gCapturingHotkey) {
        if (wParam != VK_ESCAPE) {
            settings().hotkey = int(wParam);
            settings().save();
        }
        gCapturingHotkey = false;
        return true;
    }
    bool repeat = (lParam & (1 << 30)) != 0;
    if (int(wParam) == settings().hotkey.load()) {
        if (!repeat) requestToggle(("hotkey vk " + std::to_string(wParam)).c_str());
        return true;
    }
    return false;
}

bool blocksGameInput() { return gOpen && settings().pauseInputWhileOpen; }

} // namespace modmenu::ui
