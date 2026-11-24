#pragma once
#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <string_view>
#include <vector>
namespace modmenu::workspace {
struct Caption {
    std::map<std::string, std::string> values;
    std::string get(std::string const& language) const;
};
struct Entry {
    std::string id, target;
    Caption label;
    bool external{};
};
struct EntryDefinition {
    bool declared{}, valid{};
    Caption label;
    std::vector<Entry> entries;
    std::string error;
    bool visible() const {
        return declared && valid && !entries.empty();
    }
};
bool identifier(std::string_view value);
bool validText(std::string_view value, std::size_t maxBytes, bool empty = false);
Caption parseCaption(nlohmann::json const& value, std::size_t maxBytes = 160);
EntryDefinition parseEntry(std::string const& document);
} // namespace modmenu::workspace
