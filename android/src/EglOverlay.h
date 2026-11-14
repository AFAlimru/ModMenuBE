#pragma once

namespace modmenu::android::overlay {

// Hooks eglSwapBuffers and eglSwapBuffersWithDamageKHR/EXT (libEGL.so) and draws the menu (ui::drawFrame) on top
// of the game frame while it is open. Also watches libvulkan, only to tell a Vulkan-rendered game apart.
bool install();

// True once the game was seen presenting through Vulkan (vkQueuePresentKHR / asking for it).
bool gameUsesVulkan();

} // namespace modmenu::android::overlay
