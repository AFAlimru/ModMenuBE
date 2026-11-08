#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <string>

namespace modmenu {

// A mod's JSON config file opened for editing in the generic editor (used for
// mods that did not register a screen through the API). Edits stay in memory
// until save(); the first save keeps a .bak copy of the original file.
class JsonConfigDoc {
public:
    bool load(std::filesystem::path const& path);
    bool save();
    void revert();

    nlohmann::ordered_json&       data() { return data_; }
    nlohmann::ordered_json const& data() const { return data_; }
    std::filesystem::path const&  path() const { return path_; }
    std::string const&            error() const { return error_; }
    bool                          dirty() const { return data_ != original_; }
    bool                          loaded() const { return loaded_; }

private:
    std::filesystem::path  path_;
    nlohmann::ordered_json data_;
    nlohmann::ordered_json original_;
    std::string            error_;
    int                    indent_ = 4;
    bool                   loaded_ = false;
};

} // namespace modmenu
