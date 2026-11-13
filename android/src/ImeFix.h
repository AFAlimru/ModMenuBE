#pragma once

namespace modmenu::android::ime {

// LeviLauncher's hidden IME field is a "visible password" field, so most Chinese/Japanese keyboards refuse to
// compose into it. While our menu is open we switch it to a plain text field and restore it afterwards.
// UI thread only (call from the preloader input callbacks).
void update(bool menuOpen);

} // namespace modmenu::android::ime
