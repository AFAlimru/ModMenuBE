#include "core/ModCommands.h"

#include "core/Util.h"

#include <algorithm>

namespace modmenu {

ModCommands& ModCommands::instance() {
    static ModCommands inst;
    return inst;
}

void ModCommands::detected(std::string const& mod, std::string const& command, std::string const& description) {
    if (mod.empty() || command.empty()) return;
    std::lock_guard lock(mutex_);
    auto key = toLower(mod);
    for (auto const& e : entries_)
        if (e.mod == key && e.command.source == ModCommand::Source::Detected && e.command.command == command) return;
    ModCommand c;
    c.command     = command;
    c.description = description;
    c.source      = ModCommand::Source::Detected;
    entries_.push_back({key, std::move(c)});
}

void ModCommands::add(std::string const& mod, ModCommand command) {
    if (mod.empty() || command.command.empty()) return;
    command.source = ModCommand::Source::Api;
    std::lock_guard lock(mutex_);
    entries_.push_back({toLower(mod), std::move(command)});
}

void ModCommands::clearApi(std::string const& mod) {
    std::lock_guard lock(mutex_);
    auto key = toLower(mod);
    std::erase_if(entries_, [&](Entry const& e) { return e.mod == key && e.command.source == ModCommand::Source::Api; });
}

std::vector<ModCommand> ModCommands::forMod(std::string const& mod) const {
    std::vector<ModCommand> out;
    auto                    key = toLower(mod);
    std::lock_guard         lock(mutex_);
    for (auto source : {ModCommand::Source::Api, ModCommand::Source::Detected})
        for (auto const& e : entries_)
            if (e.mod == key && e.command.source == source) out.push_back(e.command);
    return out;
}

} // namespace modmenu
