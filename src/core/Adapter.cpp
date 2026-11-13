#include "core/Adapter.h"

#include "core/I18n.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Util.h"

#include <algorithm>
#include <cctype>

namespace modmenu {

namespace {

using json = nlohmann::json;

std::string str(json const& j, char const* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::optional<double> num(json const& j, char const* key) {
    auto it = j.find(key);
    if (it != j.end() && it->is_number()) return it->get<double>();
    return std::nullopt;
}

AdapterOption parseOption(json const& o) {
    AdapterOption opt;
    if (o.is_string()) {
        opt.key = normalizePath(o.get<std::string>());
        return opt;
    }
    opt.key         = normalizePath(str(o, "key"));
    opt.label       = LocText::parse(o.value("label", json()));
    opt.description = LocText::parse(o.value("description", json()));
    opt.min         = num(o, "min");
    opt.max         = num(o, "max");
    opt.step        = num(o, "step");
    opt.format      = str(o, "format");
    opt.readOnly    = o.value("readOnly", false);
    if (o.contains("choices") && o["choices"].is_array()) {
        for (auto const& c : o["choices"]) {
            AdapterChoice choice;
            if (c.is_object() && c.contains("value")) {
                choice.value = nlohmann::ordered_json::parse(c["value"].dump());
                choice.label = LocText::parse(c.value("label", json()));
            } else {
                choice.value = nlohmann::ordered_json::parse(c.dump());
            }
            if (choice.label.empty())
                choice.label.values["en_US"] = choice.value.is_string() ? choice.value.get<std::string>() : choice.value.dump();
            opt.choices.push_back(std::move(choice));
        }
    }
    return opt;
}

AdapterFile parseFile(json const& f) {
    AdapterFile file;
    file.path            = str(f, "path");
    file.title           = LocText::parse(f.value("title", json()));
    file.restartRequired = f.value("restartRequired", true);
    file.listedOnly      = f.value("listedOnly", false);
    if (f.contains("hidden") && f["hidden"].is_array()) {
        for (auto const& h : f["hidden"])
            if (h.is_string()) file.hidden.insert(normalizePath(h.get<std::string>()));
    }
    if (f.contains("sections") && f["sections"].is_array()) {
        for (auto const& s : f["sections"]) {
            if (!s.is_object()) continue;
            AdapterSection section;
            section.title       = LocText::parse(s.value("title", json()));
            section.description = LocText::parse(s.value("description", json()));
            if (s.contains("options") && s["options"].is_array()) {
                for (auto const& o : s["options"]) {
                    auto opt = parseOption(o);
                    if (!opt.key.empty()) section.options.push_back(std::move(opt));
                }
            }
            file.sections.push_back(std::move(section));
        }
    }
    return file;
}

std::shared_ptr<Adapter const> parseAdapter(std::filesystem::path const& path) {
    std::string text;
    if (!readFile(path, text)) return nullptr;
    auto j = json::parse(text, nullptr, false, true);
    if (!j.is_object()) {
        log::warn("{} is not valid JSON; ignored", pathUtf8(path));
        return nullptr;
    }
    auto a    = std::make_shared<Adapter>();
    a->source = path;
    if (j.contains("config")) {
        auto const& c = j["config"];
        if (c.is_boolean() && !c.get<bool>()) {
            a->configDisabled = true;
        } else if (c.is_object() && c.contains("files") && c["files"].is_array()) {
            for (auto const& f : c["files"]) {
                if (f.is_object() && !str(f, "path").empty()) a->files.push_back(parseFile(f));
            }
        } else if (c.is_array()) {
            for (auto const& f : c) {
                if (f.is_object() && !str(f, "path").empty()) a->files.push_back(parseFile(f));
            }
        }
    }
    if (j.contains("open") && j["open"].is_object()) {
        auto const& o = j["open"];
        a->open.present = true;
        a->open.delayMs = std::clamp(o.value("delayMs", 150), 0, 2000);
        a->open.label   = LocText::parse(o.value("label", json()));
        if (o.contains("hotkey")) {
            auto const& h = o["hotkey"];
            if (h.is_string()) a->open.hotkey = h.get<std::string>();
            else if (h.is_object()) {
                a->open.file   = str(h, "file");
                a->open.key    = normalizePath(str(h, "key"));
                a->open.hotkey = str(h, "default");
            }
        }
        if (a->open.hotkey.empty() && a->open.key.empty()) a->open.present = false;
    }
    if (j.contains("commands") && j["commands"].is_array()) {
        for (auto const& c : j["commands"]) {
            AdapterCommand cmd;
            if (c.is_string()) cmd.command = c.get<std::string>();
            else if (c.is_object()) {
                cmd.command     = str(c, "command");
                cmd.label       = LocText::parse(c.value("label", json()));
                cmd.description = LocText::parse(c.value("description", json()));
            }
            while (!cmd.command.empty() && (cmd.command.front() == '/' || cmd.command.front() == ' '))
                cmd.command.erase(cmd.command.begin());
            // One line only: a newline would send the chat early.
            if (cmd.command.empty() || cmd.command.find_first_of("\r\n") != std::string::npos) continue;
            a->commands.push_back(std::move(cmd));
        }
    }
    a->metadata = j;
    a->metadata.erase("open");
    a->metadata.erase("commands");
    a->metadata.erase("config");
    a->metadata.erase("$schema");
    return a;
}

} // namespace

std::string LocText::get() const {
    if (values.empty()) return {};
    auto lang = activeLanguage();
    if (auto it = values.find(lang); it != values.end()) return it->second;
    // zh_TW falls back to any Chinese before English.
    if (lang.rfind("zh", 0) == 0) {
        for (auto const& [k, v] : values)
            if (k.rfind("zh", 0) == 0) return v;
    }
    if (auto it = values.find("en_US"); it != values.end()) return it->second;
    if (auto it = values.find("en"); it != values.end()) return it->second;
    return values.begin()->second;
}

LocText LocText::parse(json const& j) {
    LocText t;
    if (j.is_string()) t.values["en_US"] = j.get<std::string>();
    else if (j.is_object()) {
        for (auto const& [k, v] : j.items())
            if (v.is_string()) t.values[k] = v.get<std::string>();
    }
    return t;
}

AdapterFile const* Adapter::fileFor(std::filesystem::path const& relative) const {
    auto want = toLower(pathUtf8(relative.lexically_normal().generic_wstring()));
    for (auto const& f : files) {
        auto have = toLower(pathUtf8(std::filesystem::path(fromUtf8(f.path)).lexically_normal().generic_wstring()));
        if (have == want) return &f;
    }
    return nullptr;
}

std::shared_ptr<Adapter const> loadAdapter(std::string const& modName, std::filesystem::path const& modFolder) {
    std::error_code ec;
    auto own = modFolder / L"modmenu.json";
    if (std::filesystem::is_regular_file(own, ec)) return parseAdapter(own);
    auto community = paths::selfDir() / L"adapters" / (fromUtf8(modName) + L".json");
    if (std::filesystem::is_regular_file(community, ec)) return parseAdapter(community);
    return nullptr;
}

std::string humanizeKey(std::string const& key) {
    std::string last = key.substr(key.find_last_of('.') == std::string::npos ? 0 : key.find_last_of('.') + 1);
    std::string out;
    for (size_t i = 0; i < last.size(); ++i) {
        char c = last[i];
        if (c == '_' || c == '-') {
            out += ' ';
            continue;
        }
        bool upper = std::isupper(uint8_t(c)) != 0;
        bool prevLower = i > 0 && std::islower(uint8_t(last[i - 1]));
        bool nextLower = i + 1 < last.size() && std::islower(uint8_t(last[i + 1]));
        bool prevUpper = i > 0 && std::isupper(uint8_t(last[i - 1]));
        if (upper && i > 0 && (prevLower || (prevUpper && nextLower))) out += ' ';
        out += (i == 0) ? char(std::toupper(uint8_t(c))) : (upper && nextLower ? char(std::tolower(uint8_t(c))) : c);
    }
    return out;
}

std::string normalizePath(std::string const& key) {
    if (key.empty() || key[0] != '/') return key;
    std::string out;
    for (size_t i = 1; i < key.size(); ++i) {
        if (key[i] == '/') out += '.';
        else if (key[i] == '~' && i + 1 < key.size()) out += key[++i] == '1' ? '/' : '~';
        else out += key[i];
    }
    return out;
}

nlohmann::ordered_json* resolvePath(nlohmann::ordered_json& root, std::string const& key) {
    nlohmann::ordered_json* node = &root;
    size_t                  pos  = 0;
    while (pos <= key.size()) {
        size_t      dot  = key.find('.', pos);
        std::string part = key.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
        if (node->is_object()) {
            auto it = node->find(part);
            if (it == node->end()) return nullptr;
            node = &*it;
        } else if (node->is_array()) {
            char* end   = nullptr;
            long  index = std::strtol(part.c_str(), &end, 10);
            if (part.empty() || *end || index < 0 || size_t(index) >= node->size()) return nullptr;
            node = &(*node)[size_t(index)];
        } else {
            return nullptr;
        }
        if (dot == std::string::npos) break;
        pos = dot + 1;
    }
    return node;
}

} // namespace modmenu
