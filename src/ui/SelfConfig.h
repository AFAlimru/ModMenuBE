#pragma once

namespace modmenu::ui {

// (Re)registers ModMenuBE's own config screen in the current language.
void registerSelfConfig();

#ifndef _WIN32
// Android (android/src/Entry.cpp): the MOD button is a LeviLauncher floating button.
void openButtonLayout();
void resetButtonLayout();
#endif

} // namespace modmenu::ui
