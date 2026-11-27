// Real MinHook on an inert exact game mapping, never executing any game code.
// The native loader/reader originals are replaced with fixture callbacks. An
// engine definition event is interleaved at the earliest hook activation.
#include <Windows.h>
#include <MinHook.h>
#include <string>
#include <iostream>
namespace fixture {
HMODULE image{};
std::string captured;
int events{}, creates{};
std::wstring failureMode;
bool applyFailed{};
HMODULE WINAPI module(LPCWSTR name) {
    return name ? ::GetModuleHandleW(name) : image;
}
void load(void*, void const*);
unsigned char parse(void*, char const*, std::size_t, void*, char);
void definitionEvent();
bool patched(unsigned rva) {
    return reinterpret_cast<unsigned char*>(image)[rva] == 0xE9;
}
MH_STATUS WINAPI create(LPVOID target, LPVOID detour, LPVOID* original) {
    ++creates;
    auto status = ::MH_CreateHook(target, detour, original);
    if (status == MH_OK) {
        auto rva = reinterpret_cast<std::uintptr_t>(target) - reinterpret_cast<std::uintptr_t>(image);
        if (rva == 0x416F40)
            *original = reinterpret_cast<void*>(load);
        if (rva == 0xE033870)
            *original = reinterpret_cast<void*>(parse);
    }
    return status;
}
MH_STATUS WINAPI enable(LPVOID target) {
    auto status = ::MH_EnableHook(target);
    if (status == MH_OK && target == reinterpret_cast<unsigned char*>(image) + 0x416F40)
        definitionEvent();
    return status;
}
MH_STATUS WINAPI queue(LPVOID target) {
    if (failureMode == L"queue-fail" && target == reinterpret_cast<unsigned char*>(image) + 0xE033870)
        return MH_ERROR_MEMORY_ALLOC;
    return ::MH_QueueEnableHook(target);
}
MH_STATUS WINAPI apply() {
    if (!applyFailed && (failureMode == L"apply-fail" || failureMode == L"partial-apply")) {
        applyFailed = true;
        if (failureMode == L"partial-apply") {
            ::MH_EnableHook(reinterpret_cast<unsigned char*>(image) + 0x416F40);
            ::MH_EnableHook(reinterpret_cast<unsigned char*>(image) + 0xE033870);
        }
        return MH_ERROR_MEMORY_PROTECT;
    }
    auto status = ::MH_ApplyQueued();
    if (status == MH_OK && patched(0x416F40))
        definitionEvent();
    return status;
}
} // namespace fixture
#define GetModuleHandleW fixture::module
#define MH_CreateHook fixture::create
#define MH_EnableHook fixture::enable
#define MH_QueueEnableHook fixture::queue
#define MH_ApplyQueued fixture::apply
#include "../src/compat/NativeBridge.cpp"
#undef GetModuleHandleW
#undef MH_CreateHook
#undef MH_EnableHook
#undef MH_QueueEnableHook
#undef MH_ApplyQueued

namespace modmenu::ui {
bool isOpen() {
    return false;
}
bool blocksGameInput() {
    return false;
}
void requestOpen(std::string, bool) {}
} // namespace modmenu::ui
namespace fixture {
std::string const source =
    R"({"namespace":"start","main_button@common.button":{},"stacked_row":{"type":"panel"},"main_buttons_panel":{"controls":[{"stacked_rows":{"type":"stack_panel","controls":[{"play@start.play_button_stack":{}},{"settings@start.stacked_row":{"keep":7}}]}}]},"unknown":{"retain":true}})";
unsigned char parse(void*, char const* bytes, std::size_t length, void*, char) {
    captured.assign(bytes, length);
    return 1;
}
void load(void*, void const*) {
    using namespace modmenu::compat;
    ++events;
    // This is the real production parse path, with the exact audited caller.
    // Calling a mapped native original or executing the mapped EXE is forbidden.
    if (patched(0xE033870))
        ParseDefinition(profile->Address(0x416FC0), nullptr, source.data(), source.size(), nullptr, 0);
    else
        parse(nullptr, source.data(), source.size(), nullptr, 0);
}
void definitionEvent() {
    using namespace modmenu::compat;
    alignas(8) std::array<std::byte, 48> callback{}, record{};
    auto table = profile->Address(0xE75C408);
    char const* path = "ui/start_screen.json";
    std::size_t length = 20, capacity = 31;
    std::memcpy(callback.data(), &table, 8);
    std::memcpy(callback.data() + 8, &path, 8);
    std::memcpy(callback.data() + 24, &length, 8);
    std::memcpy(callback.data() + 32, &capacity, 8);
    void* values[]{record.data(), record.data() + record.size(), record.data() + record.size()};
    DefinitionLoadHook(callback.data(), values);
}
} // namespace fixture
int wmain(int argc, wchar_t** argv) {
    if (argc < 2 || argc > 3) {
        std::cout << "Inert native installation skipped; pass exact 26.52 game EXE and optional scenario.\n";
        return 0;
    }
    fixture::image = LoadLibraryExW(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!fixture::image)
        return 2;
    fixture::failureMode = argc == 3 ? argv[2] : L"baseline";
    auto installed = modmenu::compat::installNativeBridge();
    int failures{}, checks{};
    auto check = [&](bool value, char const* message) {
        ++checks;
        if (!value) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    };
    if (fixture::failureMode == L"baseline") {
        check(installed && modmenu::compat::nativeReady(),
              "real native group installs on the exact inert image");
        check(fixture::events == 1, "one engine load event runs at the earliest activation boundary");
        check(fixture::captured.find("mmb_button@start.main_button") != std::string::npos,
              "first engine definition is transformed without a half-enabled loader/reader pair");
        check(fixture::captured.find("\"retain\":true") != std::string::npos,
              "native parse event retains unrelated definition fields");
    } else {
        check(!installed && !modmenu::compat::nativeReady(),
              "group preparation/application failure stays inactive");
        for (auto rva : {0x416F40, 0xE033870, 0x662FBE0, 0x48560A0, 0x48560B0, 0x4819AF0})
            check(!fixture::patched(rva), "failed group leaves no active native hook or queued enable");
        check(MH_ApplyQueued() == MH_OK, "no queued native activation is left behind after failure");
        check(!fixture::patched(0x416F40) && !fixture::patched(0xE033870),
              "a later apply cannot resurrect a failed definition hook pair");
    }
    modmenu::compat::ready = false;
    for (auto rva : {0x416F40, 0xE033870, 0x662FBE0, 0x48560A0, 0x48560B0, 0x4819AF0}) {
        auto target = reinterpret_cast<unsigned char*>(fixture::image) + rva;
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
    MH_Uninitialize();
    FreeLibrary(fixture::image);
    std::cout << "Inert native install: " << checks << " checks / " << failures << " failures\n";
    return failures ? 1 : 0;
}
