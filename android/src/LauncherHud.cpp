#include "LauncherHud.h"

#include "ModMenuMod.h"
#include "platform/Jni.h"

#include <atomic>
#include <mutex>

namespace modmenu::android::launcher {

namespace {
// Uses the launcher's own "hide HUD" switch: InbuiltOverlayManager.setLauncherHudHidden(boolean), the same call
// its Toggle HUD button makes. Its tick posts the view changes to the UI thread, so any thread may call it.
constexpr char const* kManager = "org.levimc.launcher.core.mods.inbuilt.overlay.InbuiltOverlayManager";

std::mutex gMutex;
bool       gWeHid = false; // only undo what we did: the player may have hidden the HUD themselves
bool       gUnavailable = false;
} // namespace

void setHudHidden(bool hidden) {
    std::lock_guard lock(gMutex);
    if (gUnavailable || (!hidden && !gWeHid)) return;
    JNIEnv* e = jni::env();
    if (!e) return;
    e->PushLocalFrame(8);
    jclass    cls   = jni::appClass(e, kManager);
    jmethodID get   = cls ? e->GetStaticMethodID(cls, "getInstance",
                                                 "()Lorg/levimc/launcher/core/mods/inbuilt/overlay/InbuiltOverlayManager;")
                          : nullptr;
    jmethodID set   = cls ? e->GetMethodID(cls, "setLauncherHudHidden", "(Z)V") : nullptr;
    jfieldID  state = cls ? e->GetFieldID(cls, "launcherHudHidden", "Z") : nullptr;
    jobject   mgr   = get ? e->CallStaticObjectMethod(cls, get) : nullptr;
    if (!set) {
        gUnavailable = true;
        log().warn("launcher HUD cannot be hidden: {}", jni::takeException(e));
    } else if (mgr) {
        if (hidden) {
            bool already = state && e->GetBooleanField(mgr, state);
            if (!already) {
                e->CallVoidMethod(mgr, set, JNI_TRUE);
                gWeHid = true;
            }
        } else {
            e->CallVoidMethod(mgr, set, JNI_FALSE);
            gWeHid = false;
        }
    }
    if (auto ex = jni::takeException(e); !ex.empty()) log().warn("launcher HUD toggle failed: {}", ex);
    e->PopLocalFrame(nullptr);
}

} // namespace modmenu::android::launcher
