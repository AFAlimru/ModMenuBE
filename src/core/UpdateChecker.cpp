#include "core/UpdateChecker.h"

#include "core/Log.h"
#include "core/Settings.h"
#include "core/Http.h"
#include "core/LipCatalog.h"
#include "core/Util.h"

#ifdef _WIN32
#include <Windows.h>
#endif
#include <cstdlib>

#include <nlohmann/json.hpp>

#include <regex>
#include <sstream>

namespace modmenu {

namespace {

using json = nlohmann::json;

// Go module proxies escape upper-case letters as "!" + lower case.
std::string goEscape(std::string const& module) {
    std::string out;
    for (char c : module) {
        if (c >= 'A' && c <= 'Z') {
            out += '!';
            out += char(c - 'A' + 'a');
        } else {
            out += c;
        }
    }
    return out;
}

std::string stripV(std::string v) {
    if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) v.erase(0, 1);
    auto plus = v.find('+');
    if (plus != std::string::npos) v.erase(plus);
    return v;
}

bool isPrerelease(std::string const& v) { return v.find('-') != std::string::npos; }

// Picks the newest version overall and the newest one sharing major.minor with `installed`.
void pickVersions(std::vector<std::string> const& versions, ModInfo const& mod, UpdateResult& r) {
    std::string newest, compatible;
    std::string const mm = majorMinor(mod.version);
    for (auto const& raw : versions) {
        std::string v = stripV(raw);
        if (v.empty() || isPrerelease(v)) continue;
        if (newest.empty() || compareVersions(v, newest) > 0) newest = v;
        if (majorMinor(v) == mm && (compatible.empty() || compareVersions(v, compatible) > 0)) compatible = v;
    }
    r.newestAny = newest;
    r.latest    = mod.update.sameMinor ? compatible : newest;
}

void classify(ModInfo const& mod, UpdateResult& r) {
    if (r.state == UpdateResult::State::Error) return;
    if (!r.latest.empty() && compareVersions(r.latest, mod.version) > 0) {
        r.state = UpdateResult::State::Available;
    } else if (mod.update.sameMinor && !r.newestAny.empty() && compareVersions(r.newestAny, mod.version) > 0) {
        r.state = UpdateResult::State::NeedsNewerGame;
    } else if (r.latest.empty() && r.newestAny.empty()) {
        r.state = UpdateResult::State::Error;
        if (r.error.empty()) r.error = "no versions found";
    } else {
        r.state = UpdateResult::State::UpToDate;
    }
}

void checkGitHub(ModInfo const& mod, std::string repo, UpdateResult& r) {
    if (repo.rfind("github.com/", 0) == 0) repo.erase(0, 11);
    auto res = httpGet("https://api.github.com/repos/" + repo + "/releases?per_page=30",
                       L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n");
    if (res.status != 200) {
        r.state = UpdateResult::State::Error;
        r.error = res.error.empty() ? "GitHub HTTP " + std::to_string(res.status) : res.error;
        return;
    }
    auto j = json::parse(res.body, nullptr, false);
    if (!j.is_array()) {
        r.state = UpdateResult::State::Error;
        r.error = "unexpected GitHub response";
        return;
    }
    std::vector<std::string> versions;
    std::map<std::string, std::string> pages;
    for (auto const& rel : j) {
        if (!rel.is_object() || rel.value("draft", false) || rel.value("prerelease", false)) continue;
        auto tag = rel.value("tag_name", std::string());
        versions.push_back(tag);
        pages[stripV(tag)] = rel.value("html_url", std::string());
    }
    pickVersions(versions, mod, r);
    r.url = pages.count(r.latest) ? pages[r.latest] : "https://github.com/" + repo + "/releases";
    classify(mod, r);
}

void checkLip(ModInfo const& mod, std::string const& tooth, UpdateResult& r) {
    // lip resolves tooth versions through the Go module proxy.
    auto res = httpGet("https://goproxy.io/" + goEscape(tooth) + "/@v/list");
    if (res.status != 200) {
        r.state = UpdateResult::State::Error;
        r.error = res.error.empty() ? "lip/goproxy HTTP " + std::to_string(res.status) : res.error;
        return;
    }
    std::vector<std::string> versions;
    std::istringstream lines(res.body);
    for (std::string line; std::getline(lines, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!line.empty()) versions.push_back(line);
    }
    pickVersions(versions, mod, r);
    if (tooth.rfind("github.com/", 0) == 0) r.url = "https://" + tooth + "/releases";
    classify(mod, r);
}

void checkCurseForge(ModInfo const& mod, std::string const& projectId, UpdateResult& r) {
    std::string key = settings().curseforgeApiKey();
    if (key.empty()) {
#ifdef _WIN32
        wchar_t buf[256]{};
        if (GetEnvironmentVariableW(L"CURSEFORGE_API_KEY", buf, 256) > 0) key = toUtf8(buf);
#else
        if (char const* env = std::getenv("CURSEFORGE_API_KEY")) key = env;
#endif
    }
    if (key.empty()) {
        r.state = UpdateResult::State::Error;
        r.error = "CurseForge API key not set";
        return;
    }
    auto res = httpGet("https://api.curseforge.com/v1/mods/" + projectId, L"x-api-key: " + fromUtf8(key) + L"\r\n");
    if (res.status != 200) {
        r.state = UpdateResult::State::Error;
        r.error = res.error.empty() ? "CurseForge HTTP " + std::to_string(res.status) : res.error;
        return;
    }
    auto j = json::parse(res.body, nullptr, false);
    if (!j.is_object() || !j.contains("data")) {
        r.state = UpdateResult::State::Error;
        r.error = "unexpected CurseForge response";
        return;
    }
    auto const& data = j["data"];
    // CurseForge files have no version field; pull a dotted version out of the display name.
    static std::regex const versionRe(R"((\d+(?:\.\d+)+))");
    std::vector<std::string> versions;
    if (data.contains("latestFiles") && data["latestFiles"].is_array()) {
        for (auto const& f : data["latestFiles"]) {
            if (!f.is_object() || f.value("releaseType", 1) != 1) continue; // 1 = release
            std::string name = f.value("displayName", f.value("fileName", std::string()));
            std::smatch m;
            if (std::regex_search(name, m, versionRe)) versions.push_back(m[1]);
        }
    }
    pickVersions(versions, mod, r);
    if (data.contains("links") && data["links"].is_object()) r.url = data["links"].value("websiteUrl", std::string());
    classify(mod, r);
}

void checkUrl(ModInfo const& mod, std::string const& url, UpdateResult& r) {
    // Expected: {"version": "1.2.3", "url": "https://...", "versions": ["1.2.3", ...]} (versions optional)
    auto res = httpGet(url);
    if (res.status != 200) {
        r.state = UpdateResult::State::Error;
        r.error = res.error.empty() ? "HTTP " + std::to_string(res.status) : res.error;
        return;
    }
    auto j = json::parse(res.body, nullptr, false);
    if (!j.is_object()) {
        r.state = UpdateResult::State::Error;
        r.error = "update URL did not return a JSON object";
        return;
    }
    std::vector<std::string> versions;
    if (j.contains("versions") && j["versions"].is_array()) {
        for (auto const& v : j["versions"]) {
            if (v.is_string()) versions.push_back(v.get<std::string>());
        }
    }
    if (j.contains("version") && j["version"].is_string()) versions.push_back(j["version"].get<std::string>());
    pickVersions(versions, mod, r);
    r.url = j.value("url", std::string());
    classify(mod, r);
}

} // namespace

UpdateChecker& UpdateChecker::instance() {
    static UpdateChecker checker;
    return checker;
}

void UpdateChecker::checkAll() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) return;
    auto mods = ModRegistry::instance().mods();
    {
        std::lock_guard lock(mutex_);
        for (auto const& m : *mods) {
            UpdateResult r;
            r.state = UpdateResult::State::Checking;
            results_[m->name] = r;
        }
    }
    std::thread([this, mods] { run(mods); }).detach();
}

void UpdateChecker::run(std::shared_ptr<ModList const> mods) {
    for (auto const& m : *mods) {
        ModInfo mod = *m;
        if (mod.update.kind == UpdateSource::Kind::None) {
            // Mods listed in the lip catalogue can be checked through lip.
            LipCatalog::instance().loadBlocking();
            if (auto e = LipCatalog::instance().find(mod.name, mod.tooth)) {
                // Listed in the catalogue: lip versions; otherwise GitHub releases.
                if (e->indexed) mod.update = {UpdateSource::Kind::Lip, e->tooth, false};
                else mod.update = {UpdateSource::Kind::GitHub, e->tooth.substr(11), false};
            } else {
                std::lock_guard lock(mutex_);
                results_[m->name].state = UpdateResult::State::NoSource;
                continue;
            }
        }
        UpdateResult r = check(mod);
        if (r.state == UpdateResult::State::Error) log::warn("update check for {} failed: {}", m->name, r.error);
        else if (r.state == UpdateResult::State::Available) log::info("update for {}: {} -> {}", m->name, m->version, r.latest);
        std::lock_guard lock(mutex_);
        results_[m->name] = std::move(r);
    }
    running_ = false;
}

UpdateResult UpdateChecker::check(ModInfo const& mod) {
    UpdateResult r;
    try {
        switch (mod.update.kind) {
        case UpdateSource::Kind::GitHub: checkGitHub(mod, mod.update.target, r); break;
        case UpdateSource::Kind::Lip: checkLip(mod, mod.update.target, r); break;
        case UpdateSource::Kind::CurseForge: checkCurseForge(mod, mod.update.target, r); break;
        case UpdateSource::Kind::Url: checkUrl(mod, mod.update.target, r); break;
        default: r.state = UpdateResult::State::NoSource; break;
        }
    } catch (std::exception const& e) {
        r.state = UpdateResult::State::Error;
        r.error = e.what();
    }
    return r;
}

UpdateResult UpdateChecker::result(std::string const& mod) const {
    std::lock_guard lock(mutex_);
    auto it = results_.find(mod);
    return it == results_.end() ? UpdateResult{} : it->second;
}

int UpdateChecker::availableCount() const {
    std::lock_guard lock(mutex_);
    int n = 0;
    for (auto const& [_, r] : results_) n += r.state == UpdateResult::State::Available;
    return n;
}

} // namespace modmenu
