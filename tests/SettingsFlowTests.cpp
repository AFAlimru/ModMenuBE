#include "compat/PassiveSettings.h"
#include "compat/DraftInput.h"
#include "compat/SettingsService.h"
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace menu;
static void check(bool value, char const *message) {
    if (!value)
        throw std::runtime_error(message);
}
int main() {
    try {
        auto root = std::filesystem::temp_directory_path() /
                    ("modmenu-local-" + std::to_string(GetCurrentProcessId()));
        struct Cleanup {
            std::filesystem::path root;
            ~Cleanup() {
                std::error_code ec;
                std::filesystem::remove_all(root, ec);
            }
        } cleanup{root};
        std::filesystem::create_directories(root / "Example");
        auto write = [](auto const &p, std::string const &s) {
            std::ofstream f(p, std::ios::binary);
            f << s;
        };
        write(root / "Example" / "manifest.json",
              R"({"name":"Example","entry":"Example.dll","version":"1.0.0"})");
        auto description =
            R"({"schemaVersion":5,"plugin":{"id":"Example","entry":"Example.dll","versions":["1.0.0"]},"config":{"file":"config.json","reload":"hot"},"fields":[{"path":"/enabled","type":"boolean","default":true,"label":"Enabled"},{"path":"/key","type":"binding","default":"F6","label":"Key"},{"path":"/scale","type":"slider","numberType":"decimal","default":0.5,"label":"Scale","min":0.0,"max":1.0,"step":0.1,"precision":1}]})";
        write(root / "Example" / "modmenu.settings.json", description);
        write(root / "Example" / "config.json", R"({"enabled":true,"key":"F6","scale":0.5,"untouched":7})");
        auto draft = LoadSettings(root, "Example");
        check(draft.valid, draft.message.c_str());
        modmenu::compat::DraftInput input;
        std::strcpy(input.buffer(draft.fields[2]), "invalid");
        check(!input.edit(draft.fields[2]) && !input.valid(), "invalid visible edit blocks Apply");
        check(SettingText(draft.fields[2]) == "0.5", "invalid edit preserves previous draft value");
        input.buffer(draft.fields[2])[0] = 0;
        check(input.buffer(draft.fields[2])[0] == 0, "empty edit must not repopulate itself");
        input.reset(draft.fields[2].path);
        check(input.valid(), "Reset clears invalid draft input");
        modmenu::compat::SettingsService service;
        std::vector<std::uint64_t> tickets;
        for (int i = 0; i < 16; ++i)
            tickets.push_back(service.load(root, "Example"));
        check(tickets.back() != 0 && service.load(root, "Example") == 0, "worker bounds outstanding results");
        for (auto t : tickets)
            service.discard(t);
        auto active = service.load(root, "Example");
        check(active != 0, "discard releases queue capacity");
        SettingsDraft result;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!service.take(active, result) && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(result.valid, "worker publishes loaded draft");
        for (auto t : tickets)
            check(!service.take(t, result), "discarded result cannot reappear");
        auto invalidTicket = service.load(root, "");
        bool delivered = false;
        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!(delivered = service.take(invalidTicket, result)) &&
               std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        check(delivered && !result.valid, "worker delivers errors even for an empty id");
        namespace b = modmenu::binding;
        b::Snapshot captureKeys{};
        b::Capture capture;
        capture.Begin(captureKeys);
        captureKeys[162] = true;
        check(!capture.Event(162, true, captureKeys), "Ctrl waits for primary");
        captureKeys[160] = true;
        check(!capture.Event(160, true, captureKeys), "Shift waits for primary");
        captureKeys[164] = true;
        check(!capture.Event(164, true, captureKeys), "Left Alt waits for primary");
        captureKeys['K'] = true;
        auto chord = capture.Event('K', true, captureKeys);
        check(chord && *chord == b::Binding{'K', 7}, "event capture preserves all three modifiers");
        check(b::Matches(*chord, captureKeys), "captured chord matches");
        captureKeys[164] = false;
        check(!b::Matches(*chord, captureKeys), "missing Left Alt cannot trigger full chord");
        captureKeys = {};
        capture.Begin(captureKeys);
        captureKeys[27] = true;
        auto unbound = capture.Event(27, true, captureKeys);
        check(unbound && unbound->key == 0, "Escape captures NONE");
        for (unsigned key : {1u, 2u, 4u, 5u, 6u, 27u})
            check(!b::Allowed(key), "mouse and Escape forbidden as primary");
        auto original = draft.original;
        draft.fields[0].value = false;
        check(LoadSettings(root, "Example").original == original, "editing or cancel must not save");
        check(SetBinding(draft.fields[1],
                         {'K', modmenu::binding::Ctrl | modmenu::binding::Shift | modmenu::binding::LeftAlt}),
              "full chord");
        check(SetSliderText(draft.fields[2], "0.7"), "decimal step");
        auto saved = ApplySettings(root, draft);
        check(saved.saved, saved.message.c_str());
        check(saved.original.find("CTRL+SHIFT+LALT+K") != std::string::npos, "canonical chord saved");
        check(saved.original.find("0.7") != std::string::npos, "decimal preserved");
        check(saved.original.find("untouched") != std::string::npos, "unknown config key preserved");
        write(root / "Example" / "config.json", R"({"enabled":true,"key":"F7","scale":0.5})");
        check(!ApplySettings(root, saved).saved, "concurrent external change refused");
        std::cout << "PASS passive transactions, decimal slider, and chord capture\n";
        return 0;
    } catch (std::exception const &e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
