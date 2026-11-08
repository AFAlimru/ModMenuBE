#pragma once

namespace modmenu::overlay {

// Keeps mouse look/clicks from reaching the game while the menu is open.
// Bedrock reads the mouse through MouseDevice::feed (raw deltas that the
// window message filter cannot stop). The function is found and hooked
// through LeviLamina's PreLoader, so this only works when PreLoader.dll is
// loaded; returns false otherwise.
bool installGameInputGuard();

} // namespace modmenu::overlay
