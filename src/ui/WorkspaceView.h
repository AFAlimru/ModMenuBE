#pragma once
#include "core/ModRegistry.h"
namespace modmenu::ui::workspaces {
#ifdef _WIN32
inline constexpr bool supported = true;
bool available(ModInfo const& mod);
bool open(ModInfo const& mod);
void close();
void suspend();
bool back();
void draw(float height);
// Render thread: validate retained origin/session, then release handoff only
// after all physical inputs are up. No provider callback is executed here.
bool tick(bool validOrigin, bool foreground, bool released);
bool externalActive();
bool inputSuspended();
void releaseResources();
#else
inline constexpr bool supported = false;
inline bool available(ModInfo const&) {
    return false;
}
inline bool open(ModInfo const&) {
    return false;
}
inline void close() {}
inline void suspend() {}
inline bool back() {
    return false;
}
inline void draw(float) {}
inline bool tick(bool, bool, bool) {
    return false;
}
inline bool externalActive() {
    return false;
}
inline bool inputSuspended() {
    return false;
}
inline void releaseResources() {}
#endif
} // namespace modmenu::ui::workspaces
