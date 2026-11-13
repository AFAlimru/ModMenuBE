#include "Entry.h"

#include "ModMenuMod.h"
#include "State.h"
#include "platform/Jni.h"
#include "core/Settings.h"

#include "ButtonIcons.inc"

#include <pl/ModMenu.hpp>

#include <sys/system_properties.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <span>

namespace modmenu::android::entry {

namespace {
constexpr const char* kModuleId = "modmenube";
constexpr const char* kButtonId = "modmenube.open";
bool gModule = false;
bool gButton = false;

// The MOD button copies Minecraft's touch HUD buttons (emote, chat, pause) and sits right of them, top centre.
// Measured on a phone at the default GUI scale: 24.5 dp squares, 2.5 dp apart, 78.5 dp wide in all. LeviLauncher
// pads a button view by 5 dp, so the view is the icon plus 10 dp.
constexpr int   kLayoutVersion   = 2; // bump to move every player's button to a new default once
constexpr float kIconDp          = 24.5f;
constexpr float kViewPaddingDp   = 5.0f;
constexpr float kGapDp           = 2.5f;
constexpr float kVanillaHalfSpan = 39.25f;

float density() {
    char value[PROP_VALUE_MAX]{};
    int  dpi = __system_property_get("ro.sf.lcd_density", value) > 0 ? atoi(value) : 0;
    return dpi > 0 ? dpi / 160.0f : 2.0f;
}

// Writes the button's default size and position into LeviLauncher's InbuiltModManager preferences (keys
// "external_button:<buttonId>"). `force` = also when the player already placed it.
void placeButton(bool force) {
    JNIEnv* e = jni::env();
    if (!e) return;
    e->PushLocalFrame(32);
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
        jmethodID getX    = e->GetMethodID(mgrCls, "getOverlayPositionX", "(Ljava/lang/String;I)I");
        jmethodID setPos  = e->GetMethodID(mgrCls, "setOverlayPosition", "(Ljava/lang/String;II)V");
        jmethodID setSize = e->GetMethodID(mgrCls, "setOverlayButtonSize", "(Ljava/lang/String;I)V");
        jobject   mgr     = getInstance ? e->CallStaticObjectMethod(mgrCls, getInstance, context) : nullptr;
        if (mgr && getX && setPos) {
            std::string key  = std::string("external_button:") + kButtonId;
            jstring     jkey = e->NewStringUTF(key.c_str());
            if (force || e->CallIntMethod(mgr, getX, jkey, -1) == -1) {
                // The game runs in landscape: its width is the screen's long side.
                jclass    resCls = e->FindClass("android/content/res/Resources");
                jmethodID sys    = e->GetStaticMethodID(resCls, "getSystem", "()Landroid/content/res/Resources;");
                jobject   res    = e->CallStaticObjectMethod(resCls, sys);
                jobject   dm     = e->CallObjectMethod(res, e->GetMethodID(resCls, "getDisplayMetrics",
                                                                            "()Landroid/util/DisplayMetrics;"));
                jclass    dmCls  = e->GetObjectClass(dm);
                int       w      = e->GetIntField(dm, e->GetFieldID(dmCls, "widthPixels", "I"));
                int       h      = e->GetIntField(dm, e->GetFieldID(dmCls, "heightPixels", "I"));
                float     d      = density();
                int       width  = std::max(w, h);
                int       viewPx = int(std::lround((kIconDp + 2 * kViewPaddingDp) * d));
                int       x      = int(std::lround(width / 2.0f + (kVanillaHalfSpan + kGapDp - kViewPaddingDp) * d));
                if (setSize) e->CallVoidMethod(mgr, setSize, jkey, int(std::lround(kIconDp + 2 * kViewPaddingDp)));
                e->CallVoidMethod(mgr, setPos, jkey, x, 0);
                log().info("MOD button placed at its default position ({}, 0), {} px", x, viewPx);
            }
        }
    }
    if (auto ex = jni::takeException(e); !ex.empty()) log().warn("could not place the MOD button: {}", ex);
    e->PopLocalFrame(nullptr);
}

bool registerButton() {
    auto const& modId = ModMenuMod::instance().getSelf().getId();
    // The icon is the whole button (outline, face, glyph); the launcher's own background and border are hidden
    // with fully transparent colours (0 would mean "default").
    constexpr uint32_t kClear = 0x00FFFFFF;
    return pl::modmenu::ButtonBuilder(kButtonId, "ModMenuBE")
        .moduleId(kModuleId)
        .modId(modId)
        .label("MOD")
        .behavior(pl::modmenu::ButtonBehavior::Click)
        .stylePreset(pl::modmenu::ButtonStylePreset::Keycap)
        .styleColors(kClear, kClear, kClear)
        .pngIcon(std::span<const unsigned char>(kButtonIconPng, sizeof(kButtonIconPng)))
        .onEvent([](std::string_view, pl::modmenu::ButtonEvent event, float) {
            if (event == pl::modmenu::ButtonEvent::Click) toggleMenu();
        })
        .registerButton();
}
} // namespace

void openButtonLayout() {
    closeMenu();
    pl::modmenu::requestOpenMenu(); // the player picks the HUD editor there
}

void resetButtonLayout() {
    placeButton(true);
    // The launcher reads size and position when it creates the button view: recreate it.
    if (gButton) pl::modmenu::unregisterButton(kButtonId);
    gButton = registerButton();
}

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

    bool relayout = settings().buttonLayoutVersion.load() < kLayoutVersion;
    placeButton(relayout);
    if (relayout) {
        settings().buttonLayoutVersion = kLayoutVersion;
        settings().save();
    }
    gButton = registerButton();

    log().info("Mod Menu module {}, floating button {}", gModule ? "registered" : "FAILED",
               gButton ? "registered" : "FAILED");
}

void uninstall() {
    if (gButton) pl::modmenu::unregisterButton(kButtonId);
    if (gModule) pl::modmenu::unregisterModule(kModuleId);
    gButton = gModule = false;
}

} // namespace modmenu::android::entry

namespace modmenu::ui {
void openButtonLayout() { android::entry::openButtonLayout(); }
void resetButtonLayout() { android::entry::resetButtonLayout(); }
} // namespace modmenu::ui
