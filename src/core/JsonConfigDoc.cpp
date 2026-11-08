#include "core/JsonConfigDoc.h"

#include "core/Log.h"
#include "core/Util.h"

namespace modmenu {

bool JsonConfigDoc::load(std::filesystem::path const& path) {
    path_   = path;
    loaded_ = false;
    error_.clear();
    std::string text;
    if (!readFile(path, text)) {
        error_ = "cannot read file";
        return false;
    }
    // Keep the file's indentation style when writing back.
    indent_ = 4;
    if (auto nl = text.find('\n'); nl != std::string::npos) {
        size_t i = nl + 1, n = 0;
        while (i < text.size() && text[i] == ' ') ++i, ++n;
        if (n == 2) indent_ = 2;
    }
    try {
        data_ = nlohmann::ordered_json::parse(text, nullptr, true, true);
    } catch (std::exception const& e) {
        error_ = e.what();
        return false;
    }
    original_ = data_;
    loaded_   = true;
    return true;
}

bool JsonConfigDoc::save() {
    if (!loaded_) return false;
    std::error_code ec;
    auto backup = path_;
    backup += L".bak";
    if (!std::filesystem::exists(backup, ec)) std::filesystem::copy_file(path_, backup, ec);
    if (!writeFileAtomic(path_, data_.dump(indent_) + "\n")) {
        error_ = "cannot write file";
        return false;
    }
    original_ = data_;
    log::info("saved {}", pathUtf8(path_));
    return true;
}

void JsonConfigDoc::revert() { data_ = original_; }

} // namespace modmenu
