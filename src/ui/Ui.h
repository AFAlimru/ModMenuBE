#pragma once

#include <Windows.h>

// Callbacks from the overlay into the menu. All run on the render thread
// except handleWindowMessage (window thread).
namespace modmenu::ui {

// True while something must be drawn (menu open or closing animation).
bool wantsFrame();
// Before ImGui::NewFrame: rebuild fonts if needed. Returns true if the font
// atlas changed (the overlay then recreates its device objects).
bool prepareFrame(float width, float height);
void drawFrame();
// The D3D11 device is going away; drop textures created with it.
void onDeviceLost();

// Window thread. Returns true if the message was consumed (hotkey, key capture).
bool handleWindowMessage(UINT msg, WPARAM wParam, LPARAM lParam);
// Whether keyboard/mouse input must be kept from the game right now.
bool blocksGameInput();

} // namespace modmenu::ui
