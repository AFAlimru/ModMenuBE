#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace modmenu {

// Text in several languages: {"zh_TW": "...", "en_US": "..."} or a plain string.
struct LocText {
    std::map<std::string, std::string> values;
    bool        empty() const { return values.empty(); }
    std::string get() const; // active UI language, then en_US, then any
    static LocText parse(nlohmann::json const& j);
};

struct AdapterChoice {
    nlohmann::ordered_json value;
    LocText                label;
};

struct AdapterOption {
    std::string                key; // dotted path into the file ("hud.projection.enabled") or a JSON pointer ("/a/b")
    LocText                    label;
    LocText                    description;
    std::optional<double>      min, max, step;
    std::string                format; // printf format for sliders
    std::vector<AdapterChoice> choices;
    bool                       readOnly = false;
};

struct AdapterSection {
    LocText                    title;
    LocText                    description;
    std::vector<AdapterOption> options;
};

struct AdapterFile {
    std::string                 path; // relative to the mod folder
    LocText                     title;
    bool                        restartRequired = true;
    bool                        listedOnly      = false; // only the options named in sections are shown
    std::set<std::string>       hidden;                  // dotted paths (prefixes hide whole objects)
    std::vector<AdapterSection> sections;
};

// "open": how to bring up the mod's own settings menu by pressing its hotkey.
struct AdapterOpen {
    bool        present = false;
    std::string hotkey;        // fixed chord ("Alt+M"), used when no binding is given
    std::string file, key;     // where the player's current binding is stored
    int         delayMs = 150; // wait after ModMenuBE closes before pressing
    LocText     label;         // button text, default "Config"
};

// "commands": chat commands the details page offers as buttons. On Windows ModMenuBE closes, opens the chat
// and types the command for the player.
struct AdapterCommand {
    std::string command;     // without the leading '/', e.g. "record start"
    LocText     label;       // button text, default "/<command>"
    LocText     description; // tooltip
};

// modmenu.json: how ModMenuBE presents a mod. Lives in the mod folder (the
// author's copy) or in mods/ModMenuBE/adapters/<ModName>.json (a community
// adapter for mods that do not ship one). The mod folder wins.
struct Adapter {
    std::filesystem::path    source;
    bool                     configDisabled = false; // "config": false
    AdapterOpen              open;
    std::vector<AdapterCommand> commands;
    std::vector<AdapterFile> files;
    nlohmann::json           metadata;  // same keys as the manifest's "modmenu" block
    // Generic options; empty when the file is not described.
    AdapterFile const*       fileFor(std::filesystem::path const& relative) const;
};

std::shared_ptr<Adapter const> loadAdapter(std::string const& modName, std::filesystem::path const& modFolder);

// "fillOpacityPercent" -> "Fill opacity percent"
std::string humanizeKey(std::string const& key);
// Looks up a dotted path / JSON pointer in a document; nullptr when absent.
nlohmann::ordered_json* resolvePath(nlohmann::ordered_json& root, std::string const& key);
std::string             normalizePath(std::string const& key); // JSON pointer -> dotted

} // namespace modmenu
