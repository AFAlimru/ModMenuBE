#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace modmenu {

// A chat command a mod offers, shown as a button on its details page.
struct ModCommand {
    enum class Source { Adapter, Api, Detected };

    std::string command;     // without the leading '/': "record start", or just "record" for a detected root
    std::string label;       // button text; empty = "/<command>"
    std::string description; // tooltip
    Source      source = Source::Detected;
    // A detected command is only its root (overloads are not known), so ModMenuBE types "/<root> " into the
    // chat and leaves the rest to the player and the game's autocomplete instead of sending it.
    bool        complete() const { return source != Source::Detected; }
};

// Commands that mods registered at runtime: found by watching LeviLamina's CommandRegistrar (Windows) or
// added through the C API. Commands from modmenu.json come from the mod's Adapter.
class ModCommands {
public:
    static ModCommands& instance();

    void detected(std::string const& mod, std::string const& command, std::string const& description);
    void add(std::string const& mod, ModCommand command); // C API
    void clearApi(std::string const& mod);

    // Runtime commands of `mod` (name compared without case), API ones first.
    std::vector<ModCommand> forMod(std::string const& mod) const;

private:
    struct Entry {
        std::string mod; // lower case
        ModCommand  command;
    };
    mutable std::mutex mutex_;
    std::vector<Entry> entries_;
};

} // namespace modmenu
