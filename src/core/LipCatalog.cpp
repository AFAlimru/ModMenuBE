#include "core/LipCatalog.h"

#include "core/Http.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Util.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <condition_variable>
#include <thread>

namespace modmenu {

namespace {

using json = nlohmann::json;

constexpr char const* kIndexUrl  = "https://levimc.org/mods/index.json";
constexpr char const* kDetailUrl = "https://levimc.org/mods/";
constexpr auto        kIndexMaxAge = std::chrono::hours(12);

std::condition_variable gLoadedCv;

std::filesystem::path cacheDir() { return paths::selfDir() / L"cache"; }

std::string str(json const& j, char const* key) {
    auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
}

std::string toothKey(std::string tooth) {
    tooth = toLower(tooth);
    for (char const* prefix : {"https://", "http://"}) {
        if (tooth.rfind(prefix, 0) == 0) tooth.erase(0, strlen(prefix));
    }
    if (auto hash = tooth.find('#'); hash != std::string::npos) tooth.erase(hash);
    while (!tooth.empty() && tooth.back() == '/') tooth.pop_back();
    if (tooth.find('/') != std::string::npos && tooth.rfind("github.com/", 0) != 0 && tooth.find('.') == std::string::npos)
        tooth = "github.com/" + tooth; // "owner/repo"
    return tooth;
}

// File name for a cache entry derived from a tooth path.
std::wstring safeName(std::string const& key) {
    std::string out;
    for (char c : key) out += (isalnum(uint8_t(c)) || c == '-' || c == '.') ? c : '_';
    return fromUtf8(out);
}

bool isSvg(std::string const& url) {
    auto lower = toLower(url);
    auto q     = lower.find('?');
    if (q != std::string::npos) lower.erase(q);
    return lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".svg") == 0;
}

} // namespace

LipCatalog& LipCatalog::instance() {
    static LipCatalog catalog;
    return catalog;
}

void LipCatalog::parseIndex(std::string const& text) {
    auto j = json::parse(text, nullptr, false);
    if (!j.is_object() || !j.contains("mods") || !j["mods"].is_array()) {
        log::warn("lip catalogue: unexpected index format");
        return;
    }
    std::map<std::string, LipEntry>    byTooth;
    std::map<std::string, std::string> byName;
    for (auto const& m : j["mods"]) {
        if (!m.is_object()) continue;
        LipEntry e;
        e.tooth       = str(m, "tooth");
        e.name        = str(m, "name");
        e.description = str(m, "description");
        e.owner       = str(m, "owner");
        e.iconUrl     = str(m, "icon");
        e.license     = str(m, "license");
        e.page        = str(m, "url");
        if (e.tooth.empty()) continue;
        auto key = toothKey(e.tooth);
        if (!e.name.empty()) byName.try_emplace(toLower(e.name), key);
        byTooth[key] = std::move(e);
    }
    std::lock_guard lock(mutex_);
    byTooth_ = std::move(byTooth);
    byName_  = std::move(byName);
}

void LipCatalog::load() {
    auto file = cacheDir() / L"lip-index.json";
    std::error_code ec;
    bool fresh = false;
    if (auto t = std::filesystem::last_write_time(file, ec); !ec) {
        fresh = std::filesystem::file_time_type::clock::now() - t < kIndexMaxAge;
    }
    std::string text;
    if (!fresh) {
        auto res = httpGet(kIndexUrl);
        if (res.status == 200 && json::accept(res.body)) {
            text = std::move(res.body);
            writeFileAtomic(file, text);
            log::info("lip catalogue refreshed");
        } else {
            log::warn("lip catalogue download failed ({}), using cache", res.error.empty() ? std::to_string(res.status) : res.error);
        }
    }
    if (text.empty()) readFile(file, text);
    if (!text.empty()) parseIndex(text);
    {
        std::lock_guard lock(mutex_);
        loaded_  = true;
        loading_ = false;
    }
    gLoadedCv.notify_all();
}

void LipCatalog::loadAsync() {
    {
        std::lock_guard lock(mutex_);
        if (loaded_ || loading_) return;
        loading_ = true;
    }
    std::thread([this] { load(); }).detach();
}

void LipCatalog::loadBlocking() {
    loadAsync();
    std::unique_lock lock(mutex_);
    gLoadedCv.wait_for(lock, std::chrono::seconds(30), [this] { return loaded_; });
}

bool LipCatalog::loaded() const {
    std::lock_guard lock(mutex_);
    return loaded_;
}

std::optional<LipEntry> LipCatalog::find(std::string const& modName, std::string const& tooth) const {
    std::lock_guard lock(mutex_);
    if (!tooth.empty()) {
        if (auto it = byTooth_.find(toothKey(tooth)); it != byTooth_.end()) return it->second;
    }
    if (auto n = byName_.find(toLower(modName)); n != byName_.end()) {
        if (auto it = byTooth_.find(n->second); it != byTooth_.end()) return it->second;
    }
    // Not in the index (LeviLamina itself is only reachable by its detail page):
    // a GitHub identifier is still enough for the detail page, README and avatar.
    if (!tooth.empty()) {
        std::string path = tooth;
        for (char const* prefix : {"https://", "http://"})
            if (toLower(path).rfind(prefix, 0) == 0) path.erase(0, strlen(prefix));
        if (toLower(path).rfind("github.com/", 0) != 0) path = "github.com/" + path;
        auto parts = path.substr(11);
        auto slash = parts.find('/');
        if (slash != std::string::npos && slash > 0) {
            LipEntry e;
            e.tooth = path;
            e.name  = modName;
            e.owner   = parts.substr(0, slash);
            e.indexed = false;
            return e;
        }
    }
    return std::nullopt;
}

std::shared_ptr<LipDetail const> LipCatalog::detail(std::string const& tooth) {
    auto key = toothKey(tooth);
    {
        std::lock_guard lock(mutex_);
        if (auto it = details_.find(key); it != details_.end()) return it->second;
        if (detailPending_.count(key) || detailFailed_.count(key)) return nullptr;
        detailPending_.insert(key);
    }
    // URL paths are case-sensitive: keep the identifier's own spelling.
    std::string path = tooth;
    for (char const* prefix : {"https://", "http://"}) {
        if (toLower(path).rfind(prefix, 0) == 0) path.erase(0, strlen(prefix));
    }
    if (auto hash = path.find('#'); hash != std::string::npos) path.erase(hash);
    while (!path.empty() && path.back() == '/') path.pop_back();
    if (toLower(path).rfind("github.com/", 0) != 0 && path.find('.') == std::string::npos) path = "github.com/" + path;
    std::thread([this, key, path] {
        auto res    = httpGet(std::string(kDetailUrl) + path + "/index.json", {}, 8 * 1024 * 1024);
        auto j      = json::parse(res.body, nullptr, false);
        auto detail = std::make_shared<LipDetail>();
        bool ok     = res.status == 200 && j.is_object();
        if (ok) {
            detail->description = str(j, "description");
            detail->license     = str(j, "license");
            detail->repository  = str(j, "repository");
            detail->issues     = str(j, "issues");
            if (j.contains("readmes") && j["readmes"].is_array()) {
                for (auto const& r : j["readmes"]) {
                    if (!r.is_object()) continue;
                    detail->readmes.push_back({str(r, "file"), str(r, "lang"), str(r, "text")});
                }
            }
        } else {
            log::warn("lip detail for {} failed: {}", key, res.error.empty() ? std::to_string(res.status) : res.error);
        }
        std::lock_guard lock(mutex_);
        detailPending_.erase(key);
        if (ok) details_[key] = std::move(detail);
        else detailFailed_.insert(key);
    }).detach();
    return nullptr;
}

bool LipCatalog::detailFailed(std::string const& tooth) const {
    std::lock_guard lock(mutex_);
    return detailFailed_.count(toothKey(tooth)) > 0;
}

std::filesystem::path LipCatalog::icon(LipEntry const& entry) {
    auto key = toothKey(entry.tooth);
    {
        std::lock_guard lock(mutex_);
        if (auto it = icons_.find(key); it != icons_.end()) return it->second;
        if (iconPending_.count(key)) return {};
        iconPending_.insert(key);
    }
    // WIC cannot decode SVG; the owner's GitHub avatar stands in for it.
    std::string url = entry.iconUrl;
    if ((url.empty() || isSvg(url)) && !entry.owner.empty())
        url = "https://avatars.githubusercontent.com/" + entry.owner + "?s=144";
    auto file = cacheDir() / L"icons" / (safeName(key) + L".img");
    std::thread([this, key, url, file] {
        std::error_code ec;
        bool fresh = false;
        if (auto t = std::filesystem::last_write_time(file, ec); !ec)
            fresh = std::filesystem::file_time_type::clock::now() - t < std::chrono::hours(24 * 7);
        if (!fresh && !url.empty()) {
            auto res = httpGet(url, L"Accept: image/png, image/jpeg, image/*\r\n", 8 * 1024 * 1024);
            if (res.status == 200 && !res.body.empty()) writeFileAtomic(file, res.body);
            else log::warn("icon download for {} failed: {}", key, res.error.empty() ? std::to_string(res.status) : res.error);
        }
        std::lock_guard lock(mutex_);
        if (std::filesystem::is_regular_file(file, ec)) icons_[key] = file;
        // Leave it pending on failure so we do not retry every frame.
    }).detach();
    return {};
}

} // namespace modmenu
