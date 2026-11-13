#pragma once

namespace modmenu::android::launcher {

// Hides LeviLauncher's floating HUD (every floating button, ours included, the FPS counter, the logcat
// panel) while the menu is open, and shows it again afterwards. Safe from any thread.
void setHudHidden(bool hidden);

} // namespace modmenu::android::launcher
