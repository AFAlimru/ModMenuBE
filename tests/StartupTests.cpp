// Execute the production initialization sequence with inert platform services.
// No Minecraft, DXGI hook, companion DLL or visible window is used by this test.
#include "../src/dllmain.cpp"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>

namespace fixture {
std::atomic<bool> overlayInstalled{}, guardInstalled{}, nativeResult{}, blocked{};
bool observationAvailable = true;
HANDLE entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
HANDLE released = CreateEventW(nullptr, TRUE, FALSE, nullptr);
HANDLE completed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
int failures{};
void check(bool value, char const* message) {
    if (!value) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
} // namespace fixture
namespace modmenu {
Settings& settings() {
    static Settings value;
    return value;
}
void Settings::load() {}
void reloadLanguage() {}
std::string pathUtf8(std::filesystem::path const& path) {
    return path.string();
}
ModRegistry& ModRegistry::instance() {
    static ModRegistry value;
    return value;
}
void ModRegistry::rescan() {}
std::shared_ptr<ModList const> ModRegistry::mods() const {
    return std::make_shared<ModList>();
}
namespace paths {
void setSelfModule(HMODULE) {}
std::filesystem::path selfDir() {
    return {};
}
} // namespace paths
namespace log {
void init() {}
void write(char const*, std::string const&) {}
} // namespace log
namespace ui {
void registerSelfConfig() {}
} // namespace ui
namespace overlay {
bool observeSwapChainCreation() {
    return fixture::observationAvailable;
}
bool installGameInputGuard() {
    fixture::guardInstalled = true;
    return true;
}
void watchLeviLaminaCommands() {}
bool install() {
    fixture::check(WaitForSingleObject(fixture::entered, 1000) == WAIT_OBJECT_0,
                   "native definition observation starts before the potentially slow overlay probe");
    fixture::overlayInstalled = true;
    return true;
}
void uninstall() {}
} // namespace overlay
namespace compat {
bool installNativeBridge() {
    SetEvent(fixture::entered);
    if (fixture::blocked)
        WaitForSingleObject(fixture::released, 3000);
    bool value = fixture::nativeResult;
    SetEvent(fixture::completed);
    return value;
}
} // namespace compat
} // namespace modmenu

int main() {
    using namespace fixture;
    for (int scenario = 0; scenario < 6; ++scenario) {
        overlayInstalled = false;
        guardInstalled = false;
        observationAvailable = scenario < 3;
        nativeResult = scenario % 3 == 1;
        blocked = scenario % 3 == 2;
        ResetEvent(entered);
        ResetEvent(released);
        ResetEvent(completed);
        auto worker = std::async(std::launch::async, [] { return initThreadImpl(); });
        check(WaitForSingleObject(entered, 2000) == WAIT_OBJECT_0, "optional native worker starts");
        bool finished = worker.wait_for(std::chrono::milliseconds(250)) == std::future_status::ready;
        check(finished && overlayInstalled,
              "native refusal or pending companion cannot stop overlay startup");
        check(guardInstalled, "upstream camera guard remains installed independently");
        SetEvent(released);
        worker.get();
        check(WaitForSingleObject(completed, 2000) == WAIT_OBJECT_0, "native worker completes independently");
    }
    CloseHandle(entered);
    CloseHandle(released);
    CloseHandle(completed);
    std::cout << "Production startup failures: " << failures << '\n';
    return failures ? 1 : 0;
}
