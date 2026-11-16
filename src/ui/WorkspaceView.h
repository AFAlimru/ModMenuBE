#pragma once
#include "core/ModRegistry.h"
namespace modmenu::ui::workspaces {
bool available(ModInfo const& mod);
bool open(ModInfo const& mod);
void close();
void suspend();
bool back();
void draw(float height);
// Render thread: validate retained origin/session, then release handoff only
// after all physical inputs are up. No provider callback is executed here.
bool tick(bool validOrigin,bool foreground,bool released);
bool externalActive();
bool inputSuspended();
void releaseResources();
}
