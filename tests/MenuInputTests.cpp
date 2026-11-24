#include <Windows.h>
#include "compat/NativeBridge.h"
#include "ui/Ui.h"
#include "modmenu/ModMenuBinding.h"
#include <iostream>

namespace fixture {
bool foreground = true, native = false;
modmenu::binding::Snapshot physicalKeys{};
modmenu::compat::Observation observed{true, true, 1, 100};
SHORT keyState(int key) {
    return physicalKeys[unsigned(key)] ? SHORT(0x8000) : 0;
}
HWND foregroundWindow() {
    return foreground ? reinterpret_cast<HWND>(1) : nullptr;
}
DWORD windowProcess(HWND, DWORD* pid) {
    *pid = GetCurrentProcessId();
    return 1;
}
} // namespace fixture
#define GetAsyncKeyState fixture::keyState
#define GetForegroundWindow fixture::foregroundWindow
#define GetWindowThreadProcessId fixture::windowProcess
#include "../src/ui/ModMenuScreen.cpp"
#undef GetAsyncKeyState
#undef GetForegroundWindow
#undef GetWindowThreadProcessId

namespace modmenu::compat {
bool installNativeBridge() {
    return false;
}
bool nativeReady() {
    return fixture::native;
}
Observation nativeObservation() {
    return fixture::observed;
}
std::string gameLanguage() {
    return "en_US";
}
} // namespace modmenu::compat

int main() {
    using namespace modmenu;
    int failures = 0, checks = 0;
    auto check = [&](bool value, char const* message) {
        ++checks;
        if (!value) {
            ++failures;
            std::cerr << "FAIL: " << message << '\n';
        }
    };
    settings().hotkey = VK_F8;
    settings().hotkeyModifiers = 0;
    fixture::physicalKeys[VK_F8] = true;
    check(ui::handleWindowMessage(WM_KEYDOWN, VK_F8, 0) && ui::gPending,
          "cold fallback F8 requests a frame without native admission");
    ui::gRequests = {};
    ui::gPending = false;
    fixture::physicalKeys = {};
    ui::notifyFocusLost();
    fixture::foreground = false;
    ui::pollInputRelease();
    fixture::foreground = true;
    fixture::physicalKeys['K'] = true;
    ui::pollInputRelease();
    check(!ui::gHotkeyArmed, "closed-menu recovery stays gated while any key is held");
    fixture::physicalKeys = {};
    ui::pollInputRelease();
    fixture::physicalKeys[VK_F8] = true;
    check(ui::handleWindowMessage(WM_KEYDOWN, VK_F8, 0) && ui::gPending,
          "first F8 after closed-menu focus recovery opens without needing a prior UI frame");
    ui::gRequests = {};
    ui::gPending = false;
    ui::gFocusLost = false;
    fixture::physicalKeys = {};
    ui::gOpen = true;
    ui::gInputActive = true;
    settings().pauseInputWhileOpen = true;
    check(ui::acceptsUiInput() && ui::blocksGameInput(),
          "fallback overlay remains usable without native extension");
    settings().pauseInputWhileOpen = false;
    check(!ui::blocksGameInput(), "upstream input-blocking preference remains effective");
    settings().pauseInputWhileOpen = true;
    ui::gInputActive = false;
    check(!ui::acceptsUiInput(), "hidden or release-gated UI rejects queued input");
    ui::gInputActive = true;
    fixture::foreground = false;
    check(!ui::acceptsUiInput() && !ui::blocksGameInput(),
          "focus loss releases all game and UI input ownership");
    fixture::foreground = true;
    ui::gFocusLost = true;
    check(!ui::acceptsUiInput() && !ui::blocksGameInput(),
          "focus notification immediately suspends input before next frame");
    ui::gFocusLost = false;
    fixture::native = true;
    ui::gOriginIdentity = 100;
    ui::gOriginScene = 1;
    check(ui::acceptsUiInput() && ui::blocksGameInput(), "known native origin owns input");
    fixture::observed.identity = 101;
    check(!ui::acceptsUiInput() && !ui::blocksGameInput(), "changed native identity rejects queued input");
    fixture::observed.identity = 100;
    fixture::observed.eligible = false;
    check(!ui::acceptsUiInput() && !ui::blocksGameInput(), "invalid native scene rejects queued input");
    fixture::observed.eligible = true;
    ui::gOpen = false;
    check(!ui::acceptsUiInput() && !ui::blocksGameInput(), "closed menu owns no input");
    compat::Admission admission;
    admission.observe({true, true, 1, 100});
    admission.open();
    admission.observe({true, false, 1, 100});
    check(admission.retained() && !admission.visible(), "temporary focus loss preserves menu origin");
    admission.observe({true, true, 1, 100});
    check(admission.visible(), "return to original scene resumes menu");
    admission.observe({true, true, 1, 101});
    check(!admission.retained(), "different scene identity invalidates retained page");
    compat::ReleaseGate gate;
    binding::Snapshot keys{};
    keys['K'] = true;
    gate.suspend();
    check(!gate.allow(keys), "held key cannot trigger on return");
    keys['K'] = false;
    check(gate.allow(keys), "released inputs allow safe resume");
    std::cout << "Production menu input checks: " << checks << ", failures: " << failures << '\n';
    return failures ? 1 : 0;
}
