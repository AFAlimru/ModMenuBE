#pragma once

namespace modmenu::overlay {

// Learns which mod registers which chat command by hooking LeviLamina's
// ll::command::CommandRegistrar::getOrCreateCommand, whose last argument is the owning mod. Waits in the
// background for LeviLamina.dll; does nothing without LeviLamina. Results go to ModCommands.
void watchLeviLaminaCommands();

} // namespace modmenu::overlay
