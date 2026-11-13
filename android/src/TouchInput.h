#pragma once

namespace modmenu::android::input {

// Registers the preloader touch / key / text callbacks (UI thread). While the menu is open they queue
// events for the render thread and return true so the game never sees them.
void install();

// Render thread: hands queued events to the current ImGui context.
void drain();

// UI or render thread: forget pointer state when the menu closes.
void onMenuClosed();

// Render thread: true for the frame after the last finger lifted, to clear hover.
bool consumeReleasedHover();

} // namespace modmenu::android::input
