#pragma once

namespace modmenu::android::overlay {

// Hooks eglSwapBuffers (libEGL.so) and draws the menu (ui::drawFrame) on top of the game frame while it is open.
bool install();

} // namespace modmenu::android::overlay
