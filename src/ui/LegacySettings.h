#pragma once
#include "core/ModRegistry.h"
namespace modmenu::ui::legacy {
bool available(ModInfo const&);
void open(ModInfo const&);
void close();
void suspend();
void draw(float height);
void footer(float width);
bool captureKey(unsigned key,bool down);
bool capturing();
void beginMenuBinding();
}
