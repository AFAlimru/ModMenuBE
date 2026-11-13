#include "ImeFix.h"

#include "ModMenuMod.h"

#include <jni.h>

namespace modmenu::android::ime {

namespace {

constexpr jint TYPE_CLASS_TEXT = 0x1;

bool gApplied = false;
bool gFailed = false;
jobject gInput = nullptr;  // global ref to the launcher's PreloaderTextInput
jint gOriginalType = 0;
jmethodID gSetInputType = nullptr;

JNIEnv* env() {
    JavaVM* vm = ModMenuMod::instance().getSelf().getJavaVM();
    JNIEnv* e = nullptr;
    if (!vm || vm->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_6) != JNI_OK) return nullptr;
    return e;
}

bool failed(JNIEnv* e, const char* what) {
    if (e->ExceptionCheck()) e->ExceptionClear();
    log().warn("IME fix unavailable ({}); Chinese input may not work", what);
    gFailed = true;
    return false;
}

bool apply(JNIEnv* e) {
    // Runs inside a Java -> native input callback, so FindClass sees the launcher's class loader.
    jclass stateCls = e->FindClass("org/levimc/launcher/core/minecraft/MinecraftActivityState");
    if (!stateCls) return failed(e, "MinecraftActivityState");
    jmethodID getActivity = e->GetStaticMethodID(stateCls, "getCurrentActivity", "()Landroid/app/Activity;");
    if (!getActivity) return failed(e, "getCurrentActivity");
    jobject activity = e->CallStaticObjectMethod(stateCls, getActivity);
    if (e->ExceptionCheck() || !activity) return failed(e, "no activity");

    jclass activityCls = e->GetObjectClass(activity);
    jfieldID field = e->GetFieldID(activityCls, "preloaderTextInput",
                                   "Lorg/levimc/launcher/core/minecraft/MinecraftActivity$PreloaderTextInput;");
    if (!field) return failed(e, "preloaderTextInput");
    jobject input = e->GetObjectField(activity, field);
    if (!input) return failed(e, "no text input view");

    jclass textView = e->FindClass("android/widget/TextView");
    jmethodID getInputType = textView ? e->GetMethodID(textView, "getInputType", "()I") : nullptr;
    gSetInputType = textView ? e->GetMethodID(textView, "setInputType", "(I)V") : nullptr;
    if (!getInputType || !gSetInputType) return failed(e, "TextView methods");

    gOriginalType = e->CallIntMethod(input, getInputType);
    e->CallVoidMethod(input, gSetInputType, TYPE_CLASS_TEXT);
    if (e->ExceptionCheck()) return failed(e, "setInputType");
    gInput = e->NewGlobalRef(input);

    static bool logged = false;
    if (!logged) {
        logged = true;
        log().info("IME field switched to plain text (was 0x{:x})", gOriginalType);
    }
    return true;
}

void restore(JNIEnv* e) {
    if (gInput && gSetInputType) {
        e->CallVoidMethod(gInput, gSetInputType, gOriginalType);
        if (e->ExceptionCheck()) e->ExceptionClear();
    }
    if (gInput) e->DeleteGlobalRef(gInput);
    gInput = nullptr;
}

} // namespace

void update(bool menuOpen) {
    if (gFailed || menuOpen == gApplied) return;
    JNIEnv* e = env();
    if (!e) return;
    e->PushLocalFrame(16);
    if (menuOpen) gApplied = apply(e);
    else restore(e), gApplied = false;
    e->PopLocalFrame(nullptr);
}

} // namespace modmenu::android::ime
