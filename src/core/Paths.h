#pragma once

#include <Windows.h>

#include <filesystem>

namespace modmenu::paths {

void setSelfModule(HMODULE module);
HMODULE selfModule();

// <instance>/mods/ModMenuBE
std::filesystem::path selfDir();
// <instance>/mods
std::filesystem::path modsDir();

} // namespace modmenu::paths
