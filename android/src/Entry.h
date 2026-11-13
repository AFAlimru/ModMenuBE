#pragma once

namespace modmenu::android::entry {

// Registers a launcher Mod Menu module ("ModMenuBE") and its floating button that toggles our menu.
void install();
void uninstall();

// ModMenuBE settings: open LeviLauncher's menu (HUD editor) / put the MOD button back at its default place.
void openButtonLayout();
void resetButtonLayout();

} // namespace modmenu::android::entry
