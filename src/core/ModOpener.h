#pragma once

#include <optional>
#include <string>

struct HWND__;

namespace modmenu {

// A key chord such as "Alt+M", "Ctrl+Shift+F3" or "MouseSide1".
struct Chord {
    bool     ctrl = false, alt = false, shift = false, win = false;
    unsigned vk   = 0; // virtual key; VK_MBUTTON / VK_XBUTTON1 / VK_XBUTTON2 for mouse buttons
};

// Accepts "+"-separated chords with optional spaces; modifiers Ctrl/Control,
// Alt, Shift, Win; keys A-Z, 0-9, F1-F24, Numpad0-9, named keys (Left, PageUp,
// Esc/Escape, ...), MouseMiddle, MouseSide1/2 (Mouse4/5, XButton1/2) and
// "VK_<n>" / "VK 0x<hex>". Returns nullopt for empty, null or unknown input.
std::optional<Chord> parseChord(std::string const& text);
std::string          describeChord(Chord const& chord);

// Presses the chord as real input (SendInput) so mods that read the window
// messages or poll GetAsyncKeyState both see it. Call from a worker thread.
bool sendChord(Chord const& chord, HWND__* window);

// Opens the chat (T, the game's default chat key), types "/<command>" and presses Enter. Only works in a
// world. Call from a worker thread.
// With submit = false the text is left in the chat for the player to finish.
bool sendChatCommand(std::string const& command, HWND__* window, bool submit = true);

} // namespace modmenu
