#pragma once

#include "core/ConfigRegistry.h"

// Calls into other mods' callbacks behind an SEH guard so a broken config
// screen cannot take the game down. Each returns false if the call faulted.
namespace modmenu::ui::safe {

bool getBool(ConfigOption const& o, bool& out);
bool setBool(ConfigOption const& o, bool v);
bool getInt(ConfigOption const& o, int64_t& out);
bool setInt(ConfigOption const& o, int64_t v);
bool getFloat(ConfigOption const& o, double& out);
bool setFloat(ConfigOption const& o, double v);
bool getString(ConfigOption const& o, char* out, uint32_t capacity);
bool setString(ConfigOption const& o, char const* v);
bool action(ModMenuActionFn fn, void* user);

} // namespace modmenu::ui::safe
