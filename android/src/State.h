#pragma once

#include <atomic>

namespace modmenu::android {

// Shared between the Android UI thread (input, floating button) and the render thread (eglSwapBuffers).
struct State {
    std::atomic<bool> active{false};   // between enable() and disable()
    std::atomic<bool> menuOpen{false}; // input goes to the menu, not the game
    std::atomic<long long> openedAtMs{0};  // when openMenu() ran
    std::atomic<long long> lastFrameMs{0}; // last frame the menu was drawn
};

long long nowMs();

State& state();

// Any thread.
void openMenu();
void closeMenu();
void toggleMenu();

// Render thread: the shared UI closed itself (close button, Esc/Back on the list).
void menuClosedByUi();

} // namespace modmenu::android
