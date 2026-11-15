// Separate non-game target only. This file is never linked into ModMenuBE.dll.
#include "compat/NativeBridge.h"
#include <Windows.h>
#include <filesystem>
#include "core/Log.h"
namespace modmenu::compat {
bool installNativeBridge(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);bool allowed=std::filesystem::path(path).filename()==L"TestHost.exe";log::info("Preview-only bridge allowed: {}",allowed);return allowed;}
bool nativeReady(){return true;}
Observation nativeObservation(){DWORD pid{};GetWindowThreadProcessId(GetForegroundWindow(),&pid);return {true,pid==GetCurrentProcessId(),1,1};}
std::string gameLanguage(){return "en_US";}
}
