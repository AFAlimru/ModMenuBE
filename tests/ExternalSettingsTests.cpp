#include "ModMenu/Settings.h"
#include <nlohmann/json.hpp>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <fstream>
#include <limits>
namespace {
using Json = nlohmann::json;
namespace fs = std::filesystem;
void Write(fs::path const &p, std::string const &s) {
    std::ofstream(p, std::ios::binary) << s;
}
std::string Read(fs::path const &p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}
Json Description() {
    return Json::parse(R"({
 "schemaVersion":1,
 "plugin":{"id":"ExamplePlugin","entry":"NativeExample.dll","versions":["1.0.0"]},
 "config":{"file":"settings.json","reload":"hot"},
 "fields":[
  {"path":"/enabled","type":"boolean","default":true,"label":{"en_US":"Active","zh_TW":"運作中","zh_CN":"运行中"}},
  {"path":"/limit","type":"integer","default":16,"min":1,"max":64,"step":2,"label":"Limit","category":"Timing","description":"An exact limit."},
  {"path":"/mode","type":"choice","default":1,"label":"Mode","options":["First",{"en_US":"Last","zh_TW":"最後","zh_CN":"最后"}]},
  {"path":"/key","type":"key","default":"F6","label":"Action key","keys":["NONE","F6","F7"]}
 ]})");
}
} // namespace
void ExternalSettingsChecks(void (*check)(bool, char const *)) {
    using namespace menu;
    auto root =
        fs::temp_directory_path() / (L"ModMenuExternalTests-" + std::to_wstring(GetCurrentProcessId()) +
                                     L"-" + std::to_wstring(GetTickCount64()));
    fs::create_directory(root);
    struct Cleanup {
        fs::path p;
        ~Cleanup() {
            std::error_code ec;
            fs::remove_all(p, ec);
        }
    } cleanup{root};
    auto dir = root / "ExamplePlugin";
    fs::create_directory(dir);
    auto sidecar = dir / "modmenu.settings.json";
    auto config = dir / "settings.json";
    std::string manifest = R"({"entry":"NativeExample.dll","version":"1.0.0"})";
    std::string original =
        R"({"enabled":false,"limit":8,"mode":0,"key":"F7","future":{"text":"物品","array":[1,false]}})";
    auto spec = Description();
    Write(dir / "manifest.json", manifest);
    Write(sidecar, spec.dump());
    Write(config, original);
    check(InspectSettings(root, "ExamplePlugin").available,
          "worker inspection recognizes valid external description");
    auto draft = LoadSettings(root, "ExamplePlugin");
    check(draft.valid && draft.fields.size() == 4 && Read(config) == original,
          "external description enables an unknown plugin and alternate safe config basename read-only");
    if (draft.valid && draft.fields.size() == 4) {
        check(draft.fields[1].defaultValue == SettingValue{16} && draft.fields[1].value == SettingValue{8},
              "external real default is distinct from saved numeric value");
        check(SetKey(draft.fields[3], 118) && !SetKey(draft.fields[3], 119),
              "external key allowlist accepts F7 and refuses F8");
        auto reset = draft;
        ResetSettings(reset);
        check(Read(config) == original && reset.fields[0].value == SettingValue{true},
              "external reset is draft-only");
        auto saved = ApplySettings(root, reset);
        auto result = Json::parse(Read(config));
        check(saved.saved && result["limit"] == 16 && result["mode"] == 1 && result["key"] == "F6" &&
                  result["future"]["text"] == "物品",
              "external Apply saves typed defaults and preserves unknown nested data");
        bool backup = false;
        for (auto const &f : fs::directory_iterator(dir))
            if (f.path().extension() == L".bak")
                backup |= Read(f.path()) == original;
        check(backup, "external Apply retains exact source backup");
        draft = LoadSettings(root, "ExamplePlugin");
        auto before = Read(config);
        draft.fields[1].maximum = 1000;
        draft.fields[1].value = 999;
        check(!ApplySettings(root, draft).saved && Read(config) == before,
              "external draft cannot widen authoritative bounds");
        draft = LoadSettings(root, "ExamplePlugin");
        draft.fields[2].options.push_back("Injected");
        draft.fields[2].value = 2;
        check(!ApplySettings(root, draft).saved, "external draft cannot inject a choice index");
        draft = LoadSettings(root, "ExamplePlugin");
        Write(sidecar, spec.dump(2));
        check(!ApplySettings(root, draft).saved && Read(config) == before,
              "changed description bytes refuse stale Apply even if semantic fields match");
        Write(sidecar, spec.dump());
        draft = LoadSettings(root, "ExamplePlugin");
        Write(dir / "manifest.json", manifest + " ");
        check(!ApplySettings(root, draft).saved, "changed manifest refuses stale Apply");
        Write(dir / "manifest.json", manifest);
        draft = LoadSettings(root, "ExamplePlugin");
        fs::remove(sidecar);
        check(!ApplySettings(root, draft).saved && Read(config) == before,
              "removed external description cannot redirect stale Apply to another origin");
        Write(sidecar, spec.dump());
        draft = LoadSettings(root, "ExamplePlugin");
        Write(config, original + " ");
        check(!ApplySettings(root, draft).saved && Read(config) == original + " ",
              "external target edit conflicts without overwrite");
        Write(config, original);
    }
    auto refuse = [&](Json const &candidate, char const *name) {
        Write(sidecar, candidate.dump());
        auto before = Read(config);
        check(!InspectSettings(root, "ExamplePlugin").available &&
                  !LoadSettings(root, "ExamplePlugin").valid && Read(config) == before,
              name);
    };
    for (auto field : {"schemaVersion", "plugin", "config", "fields"}) {
        auto s = spec;
        s.erase(field);
        refuse(s, "required description sections cannot be omitted");
    }
    {
        auto s = spec;
        s["schemaVersion"] = 6;
        refuse(s, "future description version refuses");
    }
    {
        auto s = spec;
        s["plugin"]["id"] = "Other";
        refuse(s, "description cannot claim another folder ID");
    }
    {
        auto s = spec;
        s["plugin"]["entry"] = "ExamplePlugin.dll";
        refuse(s, "description entry must match exact manifest entry");
    }
    {
        auto s = spec;
        s["plugin"]["versions"] = {"1.1.0"};
        refuse(s, "exact version mismatch refuses");
    }
    {
        auto s = spec;
        s["plugin"]["versions"] = {"*"};
        refuse(s, "version wildcard does not match a real manifest version");
    }
    for (auto filename : {"../outside.json", "C:/outside.json", "settings.json:stream", "CON.json",
                          "nul.json", "manifest.json", "mod.json", "modmenu.settings.json", "settings.json.",
                          "settings.JSON ", "a/b.json", ".json"}) {
        auto s = spec;
        s["config"]["file"] = filename;
        refuse(s, "unsafe or reserved external config target refuses");
    }
    {
        auto s = spec;
        s["config"]["reload"] = "execute";
        refuse(s, "description cannot request a callable reload");
    }
    {
        auto s = spec;
        s["script"] = "anything";
        refuse(s, "unknown description property refuses instead of executing");
    }
    for (auto path : {"", "/", "/a//b", "/items/0", "/bad~2escape", "relative", "/a/01"}) {
        auto s = spec;
        s["fields"][0]["path"] = path;
        refuse(s, "root array and malformed pointer edits refuse");
    }
    {
        auto s = spec;
        s["fields"][1]["path"] = "/enabled";
        refuse(s, "duplicate external field path refuses");
    }
    {
        auto s = spec;
        s["fields"][1]["path"] = "/enabled/nested";
        refuse(s, "parent child overlapping edits refuse");
    }
    {
        auto s = spec;
        s["fields"][0]["default"] = "true";
        refuse(s, "Boolean default must be a Boolean");
    }
    {
        auto s = spec;
        s["fields"][1]["default"] = 65;
        refuse(s, "numeric default must be in range");
    }
    {
        auto s = spec;
        s["fields"][1]["min"] = -1;
        refuse(s, "negative numeric range is not exposed by digit-only editor v1");
    }
    {
        auto s = spec;
        s["fields"][1]["max"] = std::numeric_limits<unsigned long long>::max();
        refuse(s, "overflowing numeric maximum refuses");
    }
    {
        auto s = spec;
        s["fields"][1]["step"] = 0;
        refuse(s, "zero numeric step refuses");
    }
    {
        auto s = spec;
        s["fields"][2]["options"] = {"same", "same"};
        refuse(s, "ambiguous duplicate choice captions refuse");
    }
    {
        auto s = spec;
        s["fields"][2]["options"] = {"a", "b", "c", "d", "e", "f", "g"};
        refuse(s, "choices exceeding compact popup capacity refuse");
    }
    {
        auto s = spec;
        s["fields"][2]["default"] = 2;
        refuse(s, "choice default must be an existing index");
    }
    {
        auto s = spec;
        s["fields"][3]["keys"] = {"ESCAPE", "F6"};
        refuse(s, "reserved unsupported key token refuses");
    }
    {
        auto s = spec;
        s["fields"][3]["default"] = "F8";
        refuse(s, "key default must belong to declared allowlist");
    }
    {
        auto s = spec;
        s["fields"][0]["label"] = {{"zh_TW", "啟用"}};
        refuse(s, "localized label requires English fallback");
    }
    {
        auto s = spec;
        s["fields"][0]["label"] = "bad\nlabel";
        refuse(s, "multiline field label refuses");
    }
    {
        auto s = spec;
        s["fields"][0]["label"] = "bad§atext";
        refuse(s, "native text formatting code refuses");
    }
    {
        auto s = spec;
        s["fields"][0]["label"] = std::string("bad\x7ftext");
        refuse(s, "DEL text control refuses");
    }
    {
        auto s = spec;
        s["fields"][1]["category"] = "General";
        refuse(s, "one category cannot silently acquire inconsistent translations");
    }
    {
        auto s = spec;
        s["fields"][0]["type"] = "script";
        refuse(s, "unsupported setting type refuses");
    }
    {
        auto s = spec;
        s["fields"][1]["min"] = 65;
        refuse(s, "reversed numeric bounds refuse");
    }
    {
        auto s = spec;
        s["fields"][0]["min"] = 0;
        refuse(s, "type-inappropriate properties refuse");
    }
    {
        auto s = spec;
        s["fields"][3]["keys"] = {"F6", "F6"};
        refuse(s, "duplicate key tokens refuse");
    }
    {
        auto s = spec;
        s["fields"] = Json::array();
        refuse(s, "empty fields refuse");
    }
    {
        auto s = spec;
        for (int n = 0; n < 62; ++n) {
            auto f = s["fields"][0];
            f["path"] = "/field" + std::to_string(n);
            s["fields"].push_back(f);
        }
        refuse(s, "more than 64 fields refuse");
    }
    Write(sidecar, "{\"schemaVersion\":1,\"schemaVersion\":1}");
    check(!LoadSettings(root, "ExamplePlugin").valid, "duplicate description members refuse");
    Write(sidecar, std::string(65537, ' '));
    check(!LoadSettings(root, "ExamplePlugin").valid, "oversized description refuses");
    Write(sidecar, std::string("{\"x\":\"\xC0\x80\"}"));
    check(!LoadSettings(root, "ExamplePlugin").valid, "invalid UTF8 description refuses");
    {
        std::string deep = "0";
        for (int n = 0; n < 40; ++n)
            deep = "{\"a\":" + deep + "}";
        Write(sidecar, deep);
        check(!LoadSettings(root, "ExamplePlugin").valid, "over-deep description refuses");
    }
    Write(sidecar, spec.dump());
    Write(config, R"({"enabled":false,"limit":8,"mode":2,"key":"F7"})");
    check(!LoadSettings(root, "ExamplePlugin").valid,
          "external config choice value cannot exceed actual options");
    Write(config, R"({"enabled":false,"limit":8,"mode":0,"key":"F8"})");
    check(!LoadSettings(root, "ExamplePlugin").valid, "external config key cannot exceed key allowlist");
    Write(config, original);
    {
        auto s = spec;
        s["fields"][1]["path"] = "/future/array/child";
        Write(sidecar, s.dump());
        check(!LoadSettings(root, "ExamplePlugin").valid, "array parents cannot be replaced by object edits");
    }
    {
        auto s = spec;
        s["fields"][1]["path"] = "/enabled/deep/child";
        Write(sidecar, s.dump());
        check(!LoadSettings(root, "ExamplePlugin").valid, "invalid distant ancestor refuses before editing");
    }
    {
        auto s = spec;
        s["fields"][1]["path"] = "/a~1b/limit";
        Write(sidecar, s.dump());
        Write(config, R"({"a/b":{"limit":9},"key":"F6"})");
        auto d = LoadSettings(root, "ExamplePlugin");
        check(d.valid && d.fields[1].value == SettingValue{9},
              "proper RFC6901 escaped named object key is supported");
    }
    Write(config, original);
    Write(sidecar, spec.dump());
    auto h = CreateFileW(sidecar.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    check(!LoadSettings(root, "ExamplePlugin").valid,
          "description already open for writing refuses authoritative read");
    if (h != INVALID_HANDLE_VALUE)
        CloseHandle(h);
    std::error_code ec;
    fs::create_hard_link(sidecar, root / "hardlink.json", ec);
    if (!ec) {
        check(!LoadSettings(root, "ExamplePlugin").valid, "multiply linked description refuses");
        fs::remove(root / "hardlink.json");
    }
    fs::remove(sidecar);
    fs::create_symlink(config, sidecar, ec);
    if (!ec) {
        check(!LoadSettings(root, "ExamplePlugin").valid, "description reparse target refuses");
        fs::remove(sidecar);
    }
    Write(sidecar, spec.dump());
    fs::remove(config);
    check(InspectSettings(root, "ExamplePlugin").available && !LoadSettings(root, "ExamplePlugin").valid &&
              !fs::exists(config),
          "missing target is never automatically created by inspection or load");
    Write(config, original);
    {
        auto s = spec;
        s["config"]["reload"] = "restart";
        Write(sidecar, s.dump());
        auto d = LoadSettings(root, "ExamplePlugin");
        auto r = ApplySettings(root, d);
        check(d.valid && r.saved && r.message.find("Restart Minecraft") != std::string::npos,
              "restart-only policy explicitly reports restart after save");
    }
    Write(sidecar, spec.dump());
    {
        auto s = spec;
        s["plugin"]["entry"] = "a.dll";
        Write(sidecar, s.dump());
        Write(dir / "manifest.json", R"({"entry":"a.dll","version":"1.0.0"})");
        check(LoadSettings(root, "ExamplePlugin").valid, "ordinary one-letter DLL basename is allowed");
        Write(dir / "manifest.json", manifest);
        Write(sidecar, spec.dump());
    }
    auto legacy = root / "AutoRestock";
    fs::create_directory(legacy);
    Write(legacy / "manifest.json", R"({"entry":"AutoRestock.dll","version":"0.3.1"})");
    Write(legacy / "config.json", R"({"enabled":true,"preRestockThreshold":17})");
    auto legacyDraft = LoadSettings(root, "AutoRestock");
    check(legacyDraft.valid, "absent sidecar retains built-in adapter");
    Write(legacy / "modmenu.settings.json", "{}");
    check(!LoadSettings(root, "AutoRestock").valid,
          "invalid present description cannot silently fall back to built-in fields");
    check(!ApplySettings(root, legacyDraft).saved,
          "new description invalidates previously opened built-in draft");
    // The shipped author examples pass through the same filesystem resolver/save
    // consumer, not a separate schema-only lookalike.
    auto examples = fs::current_path();
    while (!examples.empty() && !fs::is_directory(examples / "integration/examples")) {
        auto parent = examples.parent_path();
        if (parent == examples)
            break;
        examples = parent;
    }
    examples /= "integration/examples";
    check(fs::is_regular_file(examples / "ExamplePlugin/modmenu.settings.json"),
          "author sample is present for end-to-end verification");
    if (fs::is_regular_file(examples / "ExamplePlugin/modmenu.settings.json")) {
        Write(sidecar, Read(examples / "ExamplePlugin/modmenu.settings.json"));
        Write(dir / "manifest.json", Read(examples / "ExamplePlugin/manifest.json"));
        Write(config, Read(examples / "ExamplePlugin/settings.json"));
        auto d = LoadSettings(root, "ExamplePlugin");
        check(d.valid && d.external && d.fields.size() == 4,
              "shipped four-control template resolves through real settings storage");
        if (d.valid) {
            d.fields[1].value = 32;
            check(ApplySettings(root, d).saved && Json::parse(Read(config))["limit"] == 32,
                  "shipped template persists an ordinary typed edit");
        }
    }
    check(fs::is_regular_file(examples / "AutoRestock/modmenu.settings.json"),
          "real-plugin sidecar sample is present");
    if (fs::is_regular_file(examples / "AutoRestock/modmenu.settings.json")) {
        Write(legacy / "manifest.json", R"({"entry":"AutoRestock.dll","version":"0.3.1-test4"})");
        Write(legacy / "modmenu.settings.json", Read(examples / "AutoRestock/modmenu.settings.json"));
        auto d = LoadSettings(root, "AutoRestock");
        check(d.valid && d.external && d.fields[0].label == "Enabled (description)" &&
                  d.fields[1].defaultValue == SettingValue{6},
              "shipped real-plugin sample uses external labels and preserves audited defaults");
    }
    check(fs::is_regular_file(examples / "FastBlockBreak/modmenu.settings.json"),
          "FastBlockBreak hot-reload sample is present");
    if (fs::is_regular_file(examples / "FastBlockBreak/modmenu.settings.json")) {
        auto fast = root / "FastBlockBreak";
        fs::create_directory(fast);
        Write(fast / "manifest.json", Read(examples / "FastBlockBreak/manifest.json"));
        Write(fast / "modmenu.settings.json", Read(examples / "FastBlockBreak/modmenu.settings.json"));
        Write(fast / "config.json", Read(examples / "FastBlockBreak/config.json"));
        auto d = LoadSettings(root, "FastBlockBreak");
        check(d.valid && d.external && d.hotReload && d.fields.size() == 1 &&
                  d.fields[0].path == "/enabled" && d.fields[0].type == SettingType::Boolean &&
                  d.fields[0].defaultValue == SettingValue{true},
              "FastBlockBreak exposes one hot-reloaded Boolean enabled field");
        if (d.valid) {
            d.fields[0].value = false;
            auto saved = ApplySettings(root, d);
            auto j = Json::parse(Read(fast / "config.json"));
            check(saved.saved && j["enabled"] == false && j["future"] == "preserved",
                  "FastBlockBreak Apply changes enabled and preserves unknown config members");
            auto reloaded = LoadSettings(root, "FastBlockBreak");
            check(reloaded.valid && reloaded.fields[0].value == SettingValue{false},
                  "FastBlockBreak disabled value reloads through settings resolver");
        }
    }
}

#include <iostream>
static int failures{}, checks{};
int main() {
    ExternalSettingsChecks([](bool ok, char const *message) {
        ++checks;
        if (!ok) {
            ++failures;
            std::cerr << message << '\n';
        }
    });
    std::cout << checks << " external settings checks / " << failures << " failures\n";
    return failures ? 1 : 0;
}
