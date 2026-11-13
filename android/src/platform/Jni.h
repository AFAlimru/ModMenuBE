#pragma once

#include <jni.h>

#include <string>

namespace modmenu::android::jni {

// Call once from a thread that came from Java (mod enable()), so the launcher's class loader can be cached.
void init(JavaVM* vm);

// The calling thread's JNIEnv, attaching native threads (detached again when the thread exits).
JNIEnv* env();

// Loads an app class (dotted or slashed name) through the launcher's class loader. Local ref or nullptr.
jclass appClass(JNIEnv* e, char const* name);

// Clears a pending Java exception and returns its description ("" if none).
std::string takeException(JNIEnv* e);

// MinecraftActivityState.getCurrentActivity() as a local ref, or nullptr.
jobject currentActivity(JNIEnv* e);

} // namespace modmenu::android::jni
