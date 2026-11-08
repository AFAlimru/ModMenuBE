#include "core/ModRegistry.h"

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Util.h"

#include <Windows.h>

#include <nlohmann/json.hpp>

#include <algorithm>

namespace modmenu {

namespace {

using json = nlohmann::json;

// Well-known mods that do not carry a "modmenu" block themselves.
struct KnownMod {
    char const* name;
    char const* description;
    char const* source;
    UpdateSource::Kind kind;
    char const* target;
    bool library;
    bool sameMinor;
};

constexpr KnownMod kKnown[] = {
    {"LeviLamina", "A lightweight, modular and versatile mod loader for Minecraft Bedrock Edition.",
     "https://github.com/LiteLDev/LeviLamina", UpdateSource::Kind::Lip, "github.com/LiteLDev/LeviLamina", true, true},
    {"DearOreUI", "Runtime OreUI framework for LeviLamina client mods.", "https://github.com/copper-lamp/Dear-OreUI",
     UpdateSource::Kind::GitHub, "copper-lamp/Dear-OreUI", true, false},
    {"LHolo", "Client-side projection / hologram mod for .mcstructure and .litematic files.",
     "https://github.com/MarmieQi/LHolo", UpdateSource::Kind::GitHub, "MarmieQi/LHolo", false, true},
    {"Lamium", nullptr, "https://github.com/amatouhake/Lamium", UpdateSource::Kind::GitHub, "amatouhake/Lamium",
     false, false},
};

std::vector<std::string> stringList(json const& v) {
    std::vector<std::string> out;
    if (v.is_string()) {
        out.push_back(v.get<std::string>());
    } else if (v.is_array()) {
        for (auto const& e : v) {
            if (e.is_string()) out.push_back(e.get<std::string>());
            else if (e.is_object() && e.contains("name") && e["name"].is_string())
                out.push_back(e["name"].get<std::string>());
        }
    }
    return out;
}

std::string str(json const& j, char const* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

UpdateSource parseUpdate(json const& v) {
    UpdateSource u;
    if (v.is_string()) {
        u.kind   = UpdateSource::Kind::Url;
        u.target = v.get<std::string>();
    } else if (v.is_object()) {
        if (v.contains("github")) {
            u.kind   = UpdateSource::Kind::GitHub;
            u.target = str(v, "github");
        } else if (v.contains("lip")) {
            u.kind   = UpdateSource::Kind::Lip;
            u.target = str(v, "lip");
        } else if (v.contains("curseforge")) {
            u.kind   = UpdateSource::Kind::CurseForge;
            auto const& cf = v["curseforge"];
            u.target = cf.is_number() ? std::to_string(cf.get<long long>()) : str(v, "curseforge");
        } else if (v.contains("url")) {
            u.kind   = UpdateSource::Kind::Url;
            u.target = str(v, "url");
        }
        u.sameMinor = v.value("sameMinor", false);
    }
    if (u.target.empty()) u.kind = UpdateSource::Kind::None;
    return u;
}

// Applies one "modmenu"-style metadata object onto info.
void applyMeta(ModInfo& info, json const& meta) {
    if (!meta.is_object()) return;
    if (auto d = str(meta, "description"); !d.empty()) info.description = d;
    if (meta.contains("authors")) info.authors = stringList(meta["authors"]);
    if (meta.contains("contributors")) info.contributors = stringList(meta["contributors"]);
    if (auto l = str(meta, "license"); !l.empty()) info.license = l;
    if (auto p = str(meta, "parent"); !p.empty()) info.parent = p;
    if (auto t = str(meta, "lip"); !t.empty()) info.tooth = t;
    if (auto icon = str(meta, "icon"); !icon.empty()) {
        auto p = info.folder / fromUtf8(icon);
        std::error_code ec;
        // Keep the icon inside the mod folder.
        auto canon = std::filesystem::weakly_canonical(p, ec);
        auto base  = std::filesystem::weakly_canonical(info.folder, ec);
        if (!ec && canon.wstring().rfind(base.wstring(), 0) == 0) info.iconPath = canon;
    }
    if (meta.contains("links") && meta["links"].is_object()) {
        for (auto const& [k, v] : meta["links"].items()) {
            if (v.is_string()) info.links[k] = v.get<std::string>();
        }
    }
    if (meta.contains("badges")) {
        for (auto const& b : stringList(meta["badges"])) {
            auto lb = toLower(b);
            if (lb == "library") info.badges = info.badges | Badge::Library;
            else if (lb == "deprecated") info.badges = info.badges | Badge::Deprecated;
            else if (lb == "client") info.badges = info.badges | Badge::Client;
            else if (lb == "modpack") info.badges = info.badges | Badge::Modpack;
        }
    }
    if (meta.contains("update")) {
        if (meta["update"].is_boolean() && !meta["update"].get<bool>()) info.update = {};
        else info.update = parseUpdate(meta["update"]);
    }
    if (meta.contains("update_checker") && meta["update_checker"].is_boolean() && !meta["update_checker"].get<bool>()) {
        info.update = {};
    }
}

bool isModuleLoaded(std::filesystem::path const& dll) {
    if (dll.empty()) return false;
    return GetModuleHandleW(dll.c_str()) != nullptr;
}

std::vector<std::filesystem::path> findConfigFiles(std::filesystem::path const& folder) {
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    auto dir = folder / L"config";
    if (!std::filesystem::is_directory(dir, ec)) return out;
    for (auto it = std::filesystem::recursive_directory_iterator(dir, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (it.depth() > 2) {
            it.disable_recursion_pending();
            continue;
        }
        if (!it->is_regular_file(ec)) continue;
        auto ext = toLower(pathUtf8(it->path().extension()));
        if (ext == ".json" && it->file_size(ec) < 1024 * 1024) out.push_back(it->path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace

ModRegistry& ModRegistry::instance() {
    static ModRegistry registry;
    return registry;
}

void ModRegistry::rescan() {
    auto list = std::make_shared<ModList>();
    std::error_code ec;
    auto const modsDir = paths::modsDir();
    auto const selfDir = paths::selfDir();

    std::map<std::string, std::string> runtimeMeta;
    {
        std::lock_guard lock(mutex_);
        runtimeMeta = runtimeMeta_;
    }

    for (auto it = std::filesystem::directory_iterator(modsDir, ec);
         !ec && it != std::filesystem::directory_iterator(); it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        auto folder = it->path();
        auto open   = folder / L"manifest.json";
        auto closed = folder / L"manifest.json.close";
        bool enabled = std::filesystem::is_regular_file(open, ec);
        if (!enabled && !std::filesystem::is_regular_file(closed, ec)) continue;

        std::string text;
        if (!readFile(enabled ? open : closed, text)) continue;
        auto manifest = json::parse(text, nullptr, false, true);
        if (!manifest.is_object()) {
            log::warn("skipping {}: manifest is not valid JSON", pathUtf8(folder.filename()));
            continue;
        }

        auto info     = std::make_shared<ModInfo>();
        info->folder  = folder;
        info->name    = str(manifest, "name");
        if (info->name.empty()) info->name = pathUtf8(folder.filename());
        info->version = str(manifest, "version");
        info->type    = str(manifest, "type");
        info->entry   = str(manifest, "entry");
        info->enabled = enabled;
        info->description = str(manifest, "description");
        info->authors = stringList(manifest.contains("authors") ? manifest["authors"] : manifest.value("author", json()));
        info->self    = std::filesystem::equivalent(folder, selfDir, ec);

        if (info->type == "preload-native") info->badges = info->badges | Badge::Preload;
        else if (info->type == "native") info->badges = info->badges | Badge::LeviLamina;
        if (str(manifest, "platform") == "client") info->badges = info->badges | Badge::Client;
        if (!enabled) info->badges = info->badges | Badge::Disabled;

        for (auto const& k : kKnown) {
            if (info->name != k.name) continue;
            if (info->description.empty() && k.description) info->description = k.description;
            info->links["source"] = k.source;
            info->update          = {k.kind, k.target, k.sameMinor};
            if (k.library) info->badges = info->badges | Badge::Library;
        }

        // LeviLamina's own manifests keep extras under "extraInfo"; ours under "modmenu".
        if (manifest.contains("extraInfo")) applyMeta(*info, manifest["extraInfo"]);
        if (manifest.contains("modmenu")) applyMeta(*info, manifest["modmenu"]);
        info->adapter = loadAdapter(info->name, folder);
        if (info->adapter) {
            applyMeta(*info, info->adapter->metadata);
            log::info("adapter for {}: {} (open: {}, files: {})", info->name, pathUtf8(info->adapter->source),
                      info->adapter->open.present, info->adapter->files.size());
        }
        if (auto rm = runtimeMeta.find(info->name); rm != runtimeMeta.end()) {
            applyMeta(*info, json::parse(rm->second, nullptr, false));
        }

        // Identifier for the lip catalogue: explicit, else from the update source or a GitHub link.
        if (info->tooth.empty()) {
            if (info->update.kind == UpdateSource::Kind::Lip) info->tooth = info->update.target;
            else if (info->update.kind == UpdateSource::Kind::GitHub) info->tooth = "github.com/" + info->update.target;
            else if (auto src = info->links.find("source"); src != info->links.end()) {
                auto lower = toLower(src->second);
                if (lower.rfind("https://github.com/", 0) == 0) info->tooth = src->second.substr(8);
            }
        }

        if (info->iconPath.empty()) {
            for (auto const* candidate : {L"icon.png", L"assets/icon.png", L"pack_icon.png"}) {
                auto p = folder / candidate;
                if (std::filesystem::is_regular_file(p, ec)) {
                    info->iconPath = p;
                    break;
                }
            }
        }

        if (!info->entry.empty()) info->loaded = isModuleLoaded(folder / fromUtf8(info->entry));
        if (info->adapter && info->adapter->configDisabled) {
            // The author turned the config editor off.
        } else if (info->adapter && !info->adapter->files.empty()) {
            for (auto const& f : info->adapter->files) {
                auto p = (folder / fromUtf8(f.path)).lexically_normal();
                auto rel = p.lexically_relative(folder);
                if (!rel.empty() && rel.native()[0] != L'.' && std::filesystem::is_regular_file(p, ec))
                    info->configFiles.push_back(p);
            }
        } else {
            info->configFiles = findConfigFiles(folder);
        }
        list->push_back(std::move(info));
    }

    std::sort(list->begin(), list->end(), [](auto const& a, auto const& b) {
        return toLower(a->name) < toLower(b->name);
    });

    std::lock_guard lock(mutex_);
    for (auto const& m : *list) startupEnabled_.try_emplace(m->name, m->enabled);
    mods_ = std::move(list);
}

std::shared_ptr<ModList const> ModRegistry::mods() const {
    std::lock_guard lock(mutex_);
    return mods_;
}

std::shared_ptr<ModInfo const> ModRegistry::find(std::string const& name) const {
    auto list = mods();
    for (auto const& m : *list) {
        if (m->name == name) return m;
    }
    return nullptr;
}

bool ModRegistry::setEnabled(std::string const& name, bool enabled, std::string& error) {
    auto mod = find(name);
    if (!mod) {
        error = "mod not found";
        return false;
    }
    if (mod->self && !enabled) {
        error = "ModMenuBE cannot disable itself";
        return false;
    }
    if (mod->enabled == enabled) return true;
    auto open   = mod->folder / L"manifest.json";
    auto closed = mod->folder / L"manifest.json.close";
    auto const& from = enabled ? closed : open;
    auto const& to   = enabled ? open : closed;
    if (!MoveFileExW(from.c_str(), to.c_str(), 0)) {
        error = "rename failed (Windows error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    log::info("{} {}", enabled ? "enabled" : "disabled", name);
    rescan();
    return true;
}

bool ModRegistry::needsRestart() const {
    auto list = mods();
    std::lock_guard lock(mutex_);
    for (auto const& m : *list) {
        auto it = startupEnabled_.find(m->name);
        if (it != startupEnabled_.end() && it->second != m->enabled) return true;
    }
    return false;
}

void ModRegistry::setRuntimeMetadata(std::string const& name, std::string json) {
    {
        std::lock_guard lock(mutex_);
        runtimeMeta_[name] = std::move(json);
    }
}

void ModRegistry::clearRuntimeMetadata(std::string const& name) {
    std::lock_guard lock(mutex_);
    runtimeMeta_.erase(name);
}

bool ModRegistry::leviLaminaLoaded() const { return GetModuleHandleW(L"LeviLamina.dll") != nullptr; }

} // namespace modmenu
