#pragma once

#include <imgui.h>

#include <filesystem>

namespace modmenu::ui::icons {

// Returns the texture for a PNG, or nullptr while it is still decoding / when it failed.
// Decoding runs on a worker thread; the upload happens on the render thread.
ImTextureID get(std::filesystem::path const& png);
// Uploads finished decodes. Render thread.
void pump();
// Drops every texture (device lost / overlay teardown). Render thread.
void clear();

} // namespace modmenu::ui::icons
