#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace modmenu {

// What the LeviLamina mod catalogue (levimc.org/mods, the index lip and
// LeviLauncher use) knows about a mod.
struct LipEntry {
    std::string tooth;       // github.com/owner/repo
    std::string name;
    std::string description;
    std::string owner;
    std::string iconUrl;
    std::string license;
    std::string page;        // levimc.org page
    bool        indexed = true; // false: built from a GitHub identifier, not listed in the catalogue
};

struct LipReadme {
    std::string file;
    std::string lang;        // "en", "zh-Hans", ...
    std::string text;
};

struct LipDetail {
    std::string description;
    std::string license;
    std::string repository;
    std::string issues;
    std::vector<LipReadme> readmes;
};

class LipCatalog {
public:
    static LipCatalog& instance();

    // Loads the index (disk cache, refreshed from the network when older than
    // 12 hours) on a worker thread. Safe to call repeatedly.
    void loadAsync();
    // Same, but blocks the calling (worker) thread until done.
    void loadBlocking();
    bool loaded() const;

    // Matches by explicit tooth first, then by mod name (case-insensitive).
    std::optional<LipEntry> find(std::string const& modName, std::string const& tooth = {}) const;

    // Detail (README etc.); starts a background fetch and returns nullptr until it arrives.
    std::shared_ptr<LipDetail const> detail(std::string const& tooth);
    bool detailFailed(std::string const& tooth) const;

    // Local file of the downloaded icon, empty until it is available.
    std::filesystem::path icon(LipEntry const& entry);

private:
    void load();
    void parseIndex(std::string const& text);

    mutable std::mutex                                     mutex_;
    bool                                                   loading_ = false;
    bool                                                   loaded_  = false;
    std::map<std::string, LipEntry>                        byTooth_; // lower-case tooth
    std::map<std::string, std::string>                     byName_;  // lower-case name -> tooth key
    std::map<std::string, std::shared_ptr<LipDetail const>> details_;
    std::set<std::string>                                  detailPending_, detailFailed_;
    std::map<std::string, std::filesystem::path>           icons_;
    std::set<std::string>                                  iconPending_;
};

} // namespace modmenu
