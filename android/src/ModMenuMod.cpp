#include "ModMenuMod.h"

#include "EglOverlay.h"
#include "Entry.h"
#include "State.h"
#include "TouchInput.h"
#include "platform/Jni.h"

#include "core/I18n.h"
#include "core/Log.h"
#include "core/ModRegistry.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include "ui/MenuControl.h"
#include "ui/SelfConfig.h"

#include <pl/Input.hpp>

#include <chrono>

namespace modmenu::android {

State& state() {
    static State s;
    return s;
}

long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void openMenu() {
    if (!state().active.load()) return;
    state().openedAtMs.store(nowMs());
    state().menuOpen.store(true);
    ui::requestOpen();
}

void closeMenu() {
    if (!state().menuOpen.exchange(false)) return;
    ui::requestClose("android");
    pl::input::hideKeyboard();
    input::onMenuClosed();
}

void toggleMenu() {
    if (state().menuOpen.load()) closeMenu();
    else openMenu();
}

void menuClosedByUi() {
    if (!state().menuOpen.exchange(false)) return;
    pl::input::hideKeyboard();
    input::onMenuClosed();
}

ModMenuMod& ModMenuMod::instance() {
    static ModMenuMod inst;
    return inst;
}

ModMenuMod::ModMenuMod() : mSelf(*ll::mod::NativeMod::current()) {}

pl::log::Logger& log() { return ModMenuMod::instance().getSelf().getLogger(); }

bool ModMenuMod::load() {
    paths::setSelfDir(mSelf.getModDir());
    return true;
}

bool ModMenuMod::enable() {
    jni::init(mSelf.getJavaVM());
    settings().load();
    reloadLanguage();
    ui::registerSelfConfig();
    ModRegistry::instance().rescan();
    modmenu::log::info("ModMenuBE loaded from {} ({} mods found)", paths::selfDir().string(),
                       ModRegistry::instance().mods()->size());

    state().active.store(true);
    input::install();
    if (!overlay::install()) {
        mSelf.getLogger().error("eglSwapBuffers hook failed; the menu cannot be drawn");
    }
    entry::install();
    return true;
}

bool ModMenuMod::disable() {
    closeMenu();
    state().active.store(false);
    entry::uninstall();
    // Input callbacks cannot be unregistered; they check state().active instead.
    // The swap hook stays installed and does nothing while inactive.
    return true;
}

bool ModMenuMod::unload() { return true; }

} // namespace modmenu::android
