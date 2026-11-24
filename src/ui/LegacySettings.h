#pragma once
#include "core/ModRegistry.h"
namespace modmenu::ui::legacy {
#ifdef _WIN32
bool available(ModInfo const &);
void open(ModInfo const &);
void close();
void suspend();
void draw(float height);
void footer(float width);
bool captureKey(unsigned key, bool down);
bool capturing();
void beginMenuBinding();
#else
inline bool available(ModInfo const &) {
    return false;
}
inline void open(ModInfo const &) {}
inline void close() {}
inline void suspend() {}
inline void draw(float) {}
inline void footer(float) {}
inline bool captureKey(unsigned, bool) {
    return false;
}
inline bool capturing() {
    return false;
}
inline void beginMenuBinding() {}
#endif
} // namespace modmenu::ui::legacy
