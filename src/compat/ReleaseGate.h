#pragma once
#include "modmenu/ModMenuBinding.h"
namespace modmenu::compat {
// After loss of focus, require every physically held input to be released.
class ReleaseGate {
 bool waiting_{};
public:
 void suspend(){waiting_=true;}
 bool allow(binding::Snapshot const& keys){if(waiting_){for(bool down:keys)if(down)return false;waiting_=false;}return true;}
};
}
