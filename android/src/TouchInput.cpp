#include "TouchInput.h"

#include "core/Log.h"
#include "ui/Fonts.h"
#include "ImeFix.h"
#include "State.h"

#include <pl/Input.hpp>

#include <imgui.h>
#include <imgui_internal.h>

// Added in preloader 0.2.3. Weak so the mod still loads (without IME text) on an older launcher runtime.
namespace pl::input {
[[gnu::weak]] void registerTextInputCallback(TextInputCallback callback);
}

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace modmenu::android::input {

namespace {

// android.view.MotionEvent actions (masked)
constexpr int ACTION_DOWN         = 0;
constexpr int ACTION_UP           = 1;
constexpr int ACTION_MOVE         = 2;
constexpr int ACTION_CANCEL       = 3;
constexpr int ACTION_POINTER_DOWN = 5;
constexpr int ACTION_POINTER_UP   = 6;

// android.view.KeyEvent key codes
constexpr int KEYCODE_BACK        = 4;
constexpr int KEYCODE_DPAD_UP     = 19;
constexpr int KEYCODE_DPAD_DOWN   = 20;
constexpr int KEYCODE_DPAD_LEFT   = 21;
constexpr int KEYCODE_DPAD_RIGHT  = 22;
constexpr int KEYCODE_TAB         = 61;
constexpr int KEYCODE_ENTER       = 66;
constexpr int KEYCODE_DEL         = 67;
constexpr int KEYCODE_ESCAPE      = 111;
constexpr int KEYCODE_FORWARD_DEL = 112;
constexpr int KEYCODE_MOVE_HOME   = 122;
constexpr int KEYCODE_MOVE_END    = 123;
constexpr int KEYCODE_F8          = 138;

struct Event {
    enum class Kind { Pos, Button, Key, Char, Text } kind;
    float x{}, y{};
    bool down{};
    ImGuiKey key{ImGuiKey_None};
    unsigned int ch{};
    std::string text;
};

std::mutex gMutex;
std::vector<Event> gQueue;
bool gReleasedHover = false;

// UI thread only.
std::atomic<int> gMenuPointer{-1}; // finger that drives the ImGui "mouse"
std::set<int> gGamePointers;    // fingers that went down while the menu was closed: the game keeps them

void push(Event e) {
    std::lock_guard lock(gMutex);
    if (gQueue.size() > 512) gQueue.erase(gQueue.begin(), gQueue.begin() + 256);
    gQueue.push_back(std::move(e));
}

ImGuiKey mapKey(int code) {
    switch (code) {
    case KEYCODE_DPAD_UP: return ImGuiKey_UpArrow;
    case KEYCODE_DPAD_DOWN: return ImGuiKey_DownArrow;
    case KEYCODE_DPAD_LEFT: return ImGuiKey_LeftArrow;
    case KEYCODE_DPAD_RIGHT: return ImGuiKey_RightArrow;
    case KEYCODE_TAB: return ImGuiKey_Tab;
    case KEYCODE_ENTER: return ImGuiKey_Enter;
    case KEYCODE_DEL: return ImGuiKey_Backspace;
    case KEYCODE_FORWARD_DEL: return ImGuiKey_Delete;
    case KEYCODE_ESCAPE: return ImGuiKey_Escape;
    case KEYCODE_MOVE_HOME: return ImGuiKey_Home;
    case KEYCODE_MOVE_END: return ImGuiKey_End;
    default: return ImGuiKey_None;
    }
}

bool onTouch(pl::input::TouchEvent const& e) {
    auto& st = state();
    if (!st.active.load()) return false;
    bool open = st.menuOpen.load();
    // Never strand the player: if no game frame drew the menu since it opened (e.g. the game renders with
    // Vulkan, so there is no eglSwapBuffers to draw in), give the input back.
    if (open && st.lastFrameMs.load() < st.openedAtMs.load() && nowMs() - st.openedAtMs.load() > 1500) {
        modmenu::log::error("menu was opened but never drawn (no OpenGL ES frame?); closing it");
        closeMenu();
        open = false;
    }
    ime::update(open);
    bool isDown = e.action == ACTION_DOWN || e.action == ACTION_POINTER_DOWN;
    bool isUp   = e.action == ACTION_UP || e.action == ACTION_POINTER_UP || e.action == ACTION_CANCEL;

    if (e.action == ACTION_DOWN) gGamePointers.clear(), gMenuPointer = -1;

    if (!open) {
        if (isDown) gGamePointers.insert(e.pointerId);
        else if (isUp) gGamePointers.erase(e.pointerId);
        return false;
    }

    // A finger the game already owns keeps going to the game (so a held joystick gets its release).
    if (gGamePointers.count(e.pointerId)) {
        if (isUp) gGamePointers.erase(e.pointerId);
        return false;
    }

    if (isDown && gMenuPointer < 0) {
        gMenuPointer = e.pointerId;
        push({Event::Kind::Pos, e.x, e.y});
        push({Event::Kind::Button, e.x, e.y, true});
    } else if (e.pointerId == gMenuPointer) {
        if (e.action == ACTION_MOVE) {
            push({Event::Kind::Pos, e.x, e.y});
        } else if (isUp) {
            push({Event::Kind::Pos, e.x, e.y});
            push({Event::Kind::Button, e.x, e.y, false});
            gMenuPointer = -1;
        }
    }
    return true;
}

bool onKey(pl::input::KeyEvent const& e) {
    auto& st = state();
    if (!st.active.load()) return false;
    ime::update(st.menuOpen.load());
    // F8 toggles the menu from a hardware keyboard, like the Windows build.
    if (e.keyCode == KEYCODE_F8) {
        if (!e.isKeyDown) toggleMenu();
        return true;
    }
    if (!st.menuOpen.load()) return false;
    if (e.keyCode == KEYCODE_BACK || e.keyCode == KEYCODE_ESCAPE) {
        // Same as Esc on Windows: leave a sub view first, close from the list.
        Event ev{Event::Kind::Key};
        ev.key  = ImGuiKey_Escape;
        ev.down = e.isKeyDown;
        push(std::move(ev));
        return true;
    }
    if (ImGuiKey k = mapKey(e.keyCode); k != ImGuiKey_None) {
        Event ev{Event::Kind::Key};
        ev.key  = k;
        ev.down = e.isKeyDown;
        push(std::move(ev));
    } else if (e.isKeyDown && e.unicodeChar >= 0x20 && e.unicodeChar != 0x7F) {
        Event ev{Event::Kind::Char};
        ev.ch = e.unicodeChar;
        push(std::move(ev));
    }
    return true;
}

bool onText(pl::input::TextInputEvent const& e) {
    auto& st = state();
    if (!st.active.load() || !st.menuOpen.load()) return false;
    Event ev{Event::Kind::Text};
    ev.text = e.text;
    push(std::move(ev));
    return true;
}

} // namespace

void install() {
    static bool done = false;
    if (done) return;
    done = true;
    pl::input::registerTouchCallback(onTouch);
    pl::input::registerKeyCallback(onKey);
    if (&pl::input::registerTextInputCallback) pl::input::registerTextInputCallback(onText);
}

void drain() {
    std::vector<Event> events;
    {
        std::lock_guard lock(gMutex);
        events.swap(gQueue);
    }
    ImGuiIO& io = ImGui::GetIO();
    for (auto& e : events) {
        switch (e.kind) {
        case Event::Kind::Pos:
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMousePosEvent(e.x, e.y);
            break;
        case Event::Kind::Button:
            io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
            io.AddMouseButtonEvent(ImGuiMouseButton_Left, e.down);
            if (!e.down) gReleasedHover = true;
            break;
        case Event::Kind::Key:
            io.AddKeyEvent(e.key, e.down);
            break;
        case Event::Kind::Char: {
            char utf8[5]{};
            ImTextCharToUtf8(utf8, e.ch);
            ui::fonts::note(utf8);
            io.AddInputCharacter(e.ch);
            break;
        }
        case Event::Kind::Text:
            ui::fonts::note(e.text);
            io.AddInputCharactersUTF8(e.text.c_str());
            break;
        }
    }
}

void onMenuClosed() {
    std::lock_guard lock(gMutex);
    gQueue.clear();
    Event up{Event::Kind::Button};
    up.down = false;
    gQueue.push_back(up);
    gMenuPointer = -1;
}

bool consumeReleasedHover() {
    bool r = gReleasedHover;
    gReleasedHover = false;
    return r;
}

} // namespace modmenu::android::input
