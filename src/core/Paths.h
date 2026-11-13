#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif

#include <filesystem>

namespace modmenu::paths {

#ifdef _WIN32
void setSelfModule(HMODULE module);
HMODULE selfModule();
#else
void setSelfDir(std::filesystem::path dir);
#endif

// <instance>/mods/ModMenuBE
std::filesystem::path selfDir();
// <instance>/mods
std::filesystem::path modsDir();

} // namespace modmenu::paths
