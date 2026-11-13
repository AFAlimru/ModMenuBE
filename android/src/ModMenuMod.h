#pragma once

#include <pl/Mod.hpp>

namespace modmenu::android {

class ModMenuMod {
public:
    static ModMenuMod& instance();

    ModMenuMod();

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();
    bool unload();

private:
    ll::mod::NativeMod& mSelf;
};

pl::log::Logger& log();

} // namespace modmenu::android
