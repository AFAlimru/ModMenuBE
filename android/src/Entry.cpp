#include "Entry.h"

#include "ModMenuMod.h"
#include "State.h"
#include "platform/Jni.h"

#include <pl/ModMenu.hpp>

#include <sys/system_properties.h>

#include <cstdlib>

namespace modmenu::android::entry {

namespace {
constexpr const char* kModuleId = "modmenube";
constexpr const char* kButtonId = "modmenube.open";
bool gModule = false;
bool gButton = false;

// LeviLauncher puts a new floating button in the middle of the screen. Unless the player already moved ours,
// start it at the top left, under the launcher's own Mod Menu button. Positions live in the launcher's
// InbuiltModManager preferences under "external_button:<buttonId>".
void seedButtonPosition() {
    JNIEnv* e = jni::env();
    if (!e) return;
    e->PushLocalFrame(16);
    jobject context = jni::currentActivity(e);
    if (!context) {
        jclass    at  = e->FindClass("android/app/ActivityThread");
        jmethodID app = at ? e->GetStaticMethodID(at, "currentApplication", "()Landroid/app/Application;") : nullptr;
        context       = app ? e->CallStaticObjectMethod(at, app) : nullptr;
        jni::takeException(e);
    }
    jclass mgrCls = jni::appClass(e, "org.levimc.launcher.core.mods.inbuilt.manager.InbuiltModManager");
    if (context && mgrCls) {
        jmethodID getInstance = e->GetStaticMethodID(mgrCls, "getInstance",
                                                     "(Landroid/content/Context;)Lorg/levimc/launcher/core/mods/inbuilt/manager/InbuiltModManager;");
        jmethodID getX = e->GetMethodID(mgrCls, "getOverlayPositionX", "(Ljava/lang/String;I)I");
        jmethodID setPos = e->GetMethodID(mgrCls, "setOverlayPosition", "(Ljava/lang/String;II)V");
        jobject   mgr    = getInstance ? e->CallStaticObjectMethod(mgrCls, getInstance, context) : nullptr;
        if (mgr && getX && setPos) {
            std::string key  = std::string("external_button:") + kButtonId;
            jstring     jkey = e->NewStringUTF(key.c_str());
            if (e->CallIntMethod(mgr, getX, jkey, -1) == -1) {
                char  value[PROP_VALUE_MAX]{};
                int   dpi     = __system_property_get("ro.sf.lcd_density", value) > 0 ? atoi(value) : 0;
                float density = dpi > 0 ? dpi / 160.0f : 2.0f;
                e->CallVoidMethod(mgr, setPos, jkey, int(20 * density), int(110 * density));
                log().info("MOD button placed at its default position");
            }
        }
    }
    if (auto ex = jni::takeException(e); !ex.empty()) log().warn("could not set the MOD button position: {}", ex);
    e->PopLocalFrame(nullptr);
}
} // namespace

void install() {
    auto const& modId = ModMenuMod::instance().getSelf().getId();

    gModule = pl::modmenu::ModuleBuilder(kModuleId, "ModMenuBE")
                  .description("模組選單：已安裝模組、說明與設定。開啟後畫面上會出現 MOD 按鈕。")
                  .modId(modId)
                  .defaultEnabled(true)
                  .onToggle([](std::string_view, bool enabled) {
                      if (!enabled) closeMenu();
                  })
                  .registerModule();

    seedButtonPosition();
    gButton = pl::modmenu::ButtonBuilder(kButtonId, "ModMenuBE")
                  .moduleId(kModuleId)
                  .modId(modId)
                  .label("MOD")
                  .behavior(pl::modmenu::ButtonBehavior::Click)
                  .stylePreset(pl::modmenu::ButtonStylePreset::Accent)
                  .onEvent([](std::string_view, pl::modmenu::ButtonEvent event, float) {
                      if (event == pl::modmenu::ButtonEvent::Click) toggleMenu();
                  })
                  .registerButton();

    log().info("Mod Menu module {}, floating button {}", gModule ? "registered" : "FAILED",
               gButton ? "registered" : "FAILED");
}

void uninstall() {
    if (gButton) pl::modmenu::unregisterButton(kButtonId);
    if (gModule) pl::modmenu::unregisterModule(kModuleId);
    gButton = gModule = false;
}

} // namespace modmenu::android::entry
