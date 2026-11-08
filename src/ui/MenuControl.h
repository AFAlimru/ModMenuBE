#pragma once

#include <string>

// Thread-safe requests into the menu; the render thread applies them on its next frame.
namespace modmenu::ui {

void requestOpen(std::string mod = {}, bool openConfig = false);
void requestClose(char const* reason = "api");
void requestToggle(char const* reason = "api");
void requestRescan();
bool isOpen();

} // namespace modmenu::ui
