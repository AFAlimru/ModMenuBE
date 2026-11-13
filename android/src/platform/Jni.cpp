#include "platform/Jni.h"

#include <pthread.h>

#include <string>

namespace modmenu::android::jni {

namespace {
JavaVM*       gVm          = nullptr;
jobject       gClassLoader = nullptr; // global ref to the launcher's class loader
jmethodID     gLoadClass   = nullptr;
pthread_key_t gDetachKey;

void detachThread(void*) {
    if (gVm) gVm->DetachCurrentThread();
}
} // namespace

void init(JavaVM* vm) {
    if (gVm || !vm) return;
    gVm = vm;
    pthread_key_create(&gDetachKey, detachThread);
    JNIEnv* e = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_6) != JNI_OK) return;
    // Called from Java code, so FindClass resolves app classes here; keep their loader for other threads.
    jclass anchor = e->FindClass("org/levimc/launcher/core/minecraft/MinecraftActivityState");
    if (!anchor) {
        takeException(e);
        return;
    }
    jclass    classCls  = e->FindClass("java/lang/Class");
    jmethodID getLoader = e->GetMethodID(classCls, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject   loader    = e->CallObjectMethod(anchor, getLoader);
    if (loader) {
        gClassLoader = e->NewGlobalRef(loader);
        jclass loaderCls = e->FindClass("java/lang/ClassLoader");
        gLoadClass       = e->GetMethodID(loaderCls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    }
    takeException(e);
}

JNIEnv* env() {
    if (!gVm) return nullptr;
    JNIEnv* e = nullptr;
    jint    r = gVm->GetEnv(reinterpret_cast<void**>(&e), JNI_VERSION_1_6);
    if (r == JNI_OK) return e;
    if (r != JNI_EDETACHED) return nullptr;
    JavaVMAttachArgs args{JNI_VERSION_1_6, "ModMenuBE", nullptr};
    if (gVm->AttachCurrentThread(&e, &args) != JNI_OK) return nullptr;
    pthread_setspecific(gDetachKey, e); // non-null value => detachThread runs at thread exit
    return e;
}

jclass appClass(JNIEnv* e, char const* name) {
    if (!gClassLoader || !gLoadClass) return nullptr;
    std::string dotted = name;
    for (auto& c : dotted)
        if (c == '/') c = '.';
    jstring jname = e->NewStringUTF(dotted.c_str());
    auto    cls   = static_cast<jclass>(e->CallObjectMethod(gClassLoader, gLoadClass, jname));
    e->DeleteLocalRef(jname);
    if (e->ExceptionCheck()) {
        takeException(e);
        return nullptr;
    }
    return cls;
}

std::string takeException(JNIEnv* e) {
    if (!e->ExceptionCheck()) return {};
    jthrowable ex = e->ExceptionOccurred();
    e->ExceptionClear();
    std::string text = "Java exception";
    if (ex) {
        jclass    objCls   = e->FindClass("java/lang/Object");
        jmethodID toString = e->GetMethodID(objCls, "toString", "()Ljava/lang/String;");
        auto      js       = static_cast<jstring>(e->CallObjectMethod(ex, toString));
        if (e->ExceptionCheck()) e->ExceptionClear();
        if (js) {
            char const* chars = e->GetStringUTFChars(js, nullptr);
            if (chars) text = chars, e->ReleaseStringUTFChars(js, chars);
            e->DeleteLocalRef(js);
        }
        e->DeleteLocalRef(objCls);
        e->DeleteLocalRef(ex);
    }
    return text;
}

jobject currentActivity(JNIEnv* e) {
    jclass cls = appClass(e, "org.levimc.launcher.core.minecraft.MinecraftActivityState");
    if (!cls) return nullptr;
    jmethodID get      = e->GetStaticMethodID(cls, "getCurrentActivity", "()Landroid/app/Activity;");
    jobject   activity = get ? e->CallStaticObjectMethod(cls, get) : nullptr;
    takeException(e);
    e->DeleteLocalRef(cls);
    return activity;
}

} // namespace modmenu::android::jni
