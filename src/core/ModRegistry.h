#pragma once

#include "core/Adapter.h"

#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace modmenu {

enum class Badge : unsigned {
    None       = 0,
    Library    = 1 << 0,
    Client     = 1 << 1,
    Deprecated = 1 << 2,
    Preload    = 1 << 3, // type preload-native (loaded by LeviLauncher / PreLoader)
    LeviLamina = 1 << 4, // type native (needs LeviLamina)
    Disabled   = 1 << 5,
    Modpack    = 1 << 6,
};
inline Badge operator|(Badge a, Badge b) { return Badge(unsigned(a) | unsigned(b)); }
inline bool  has(Badge set, Badge b) { return (unsigned(set) & unsigned(b)) != 0; }

// Where to look for newer versions. Parsed from manifest "modmenu.update".
struct UpdateSource {
    enum class Kind { None, Url, GitHub, Lip, CurseForge } kind = Kind::None;
    std::string target;           // url / "owner/repo" / "github.com/owner/repo" / project id
    bool        sameMinor = false; // only versions sharing major.minor (game-bound mods)
};

struct ModInfo {
    std::string name;    // manifest "name" (also the id used by the API)
    std::string version;
    std::string type;    // preload-native / native / ...
    std::string entry;
    std::string description;
    std::vector<std::string> authors;
    std::vector<std::string> contributors;
    std::string license;
    std::string parent;  // groups this mod under another one in the list
    std::string tooth;   // lip / GitHub identifier (github.com/owner/repo) if known
    std::map<std::string, std::string> links; // homepage, source, issues, discord, ...
    std::filesystem::path folder;
    std::filesystem::path iconPath; // empty = draw a letter tile
    Badge        badges = Badge::None;
    UpdateSource update;
    bool         enabled = true;      // manifest.json (true) vs manifest.json.close (false)
    bool         loaded  = false;     // its DLL is in the process right now
    bool         self    = false;     // ModMenuBE itself
    std::vector<std::filesystem::path> configFiles; // editable JSON files (adapter list, else <folder>/config/*.json)
    std::shared_ptr<Adapter const>     adapter;     // modmenu.json, if any
};

using ModList = std::vector<std::shared_ptr<ModInfo const>>;

class ModRegistry {
public:
    static ModRegistry& instance();

    // Re-reads every mods/*/manifest.json(.close). Cheap; call on menu open.
    void rescan();
    std::shared_ptr<ModList const> mods() const;
    std::shared_ptr<ModInfo const> find(std::string const& name) const;

    // Renames manifest.json <-> manifest.json.close, the same switch LeviLauncher uses.
    // Takes effect on the next game start.
    bool setEnabled(std::string const& name, bool enabled, std::string& error);
    // Mods whose enabled state differs from what is loaded now.
    bool needsRestart() const;

    // Runtime metadata from the API (merged over the manifest on rescan).
    void setRuntimeMetadata(std::string const& name, std::string json);
    void clearRuntimeMetadata(std::string const& name);

    bool leviLaminaLoaded() const;

private:
    mutable std::mutex                       mutex_;
    std::shared_ptr<ModList const>           mods_ = std::make_shared<ModList>();
    std::map<std::string, std::string>       runtimeMeta_;
    std::map<std::string, bool>              startupEnabled_; // enabled state seen at first scan
};

} // namespace modmenu
