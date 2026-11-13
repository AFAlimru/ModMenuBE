// Android versions of the small Windows-only pieces: paths, log, opening links, guarded callbacks.
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Util.h"
#include "ui/SafeCall.h"

#include "ModMenuMod.h"
#include "platform/Jni.h"

#include <android/log.h>

namespace modmenu {

// ---------------------------------------------------------------- paths
namespace paths {
namespace {
std::filesystem::path gSelfDir;
}
void setSelfDir(std::filesystem::path dir) { gSelfDir = std::move(dir); }
std::filesystem::path selfDir() { return gSelfDir; }
std::filesystem::path modsDir() { return gSelfDir.parent_path(); }
} // namespace paths

// ---------------------------------------------------------------- log (goes to the launcher's log viewer)
namespace log {
void init() {}
void write(char const* level, std::string const& message) {
    auto& logger = android::log();
    std::string_view lv(level);
    if (lv == "ERROR") logger.error("{}", message);
    else if (lv == "WARN") logger.warn("{}", message);
    else logger.info("{}", message);
}
} // namespace log

// ---------------------------------------------------------------- links
bool openExternal(std::string const& target) {
    std::string lower = toLower(target);
    if (lower.rfind("https://", 0) != 0 && lower.rfind("http://", 0) != 0) return false;
    JNIEnv* e = android::jni::env();
    if (!e) return false;
    e->PushLocalFrame(16);
    bool ok = false;
    if (jobject activity = android::jni::currentActivity(e)) {
        jclass    uriCls    = e->FindClass("android/net/Uri");
        jmethodID parse     = e->GetStaticMethodID(uriCls, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
        jstring   jurl      = e->NewStringUTF(target.c_str());
        jobject   uri       = e->CallStaticObjectMethod(uriCls, parse, jurl);
        jclass    intentCls = e->FindClass("android/content/Intent");
        jmethodID ctor      = e->GetMethodID(intentCls, "<init>", "(Ljava/lang/String;Landroid/net/Uri;)V");
        jstring   action    = e->NewStringUTF("android.intent.action.VIEW");
        jobject   intent    = e->NewObject(intentCls, ctor, action, uri);
        jmethodID addFlags  = e->GetMethodID(intentCls, "addFlags", "(I)Landroid/content/Intent;");
        e->CallObjectMethod(intent, addFlags, 0x10000000); // FLAG_ACTIVITY_NEW_TASK
        jclass    actCls = e->FindClass("android/app/Activity");
        jmethodID start  = e->GetMethodID(actCls, "startActivity", "(Landroid/content/Intent;)V");
        e->CallVoidMethod(activity, start, intent);
        auto ex = android::jni::takeException(e);
        ok      = ex.empty();
        if (!ok) log::warn("could not open {}: {}", target, ex);
    }
    e->PopLocalFrame(nullptr);
    return ok;
}

bool openFolder(std::filesystem::path const&) { return false; } // no file manager hand-off on Android

} // namespace modmenu

// ---------------------------------------------------------------- SafeCall: no SEH on Android, call directly
namespace modmenu::ui::safe {
bool getBool(ConfigOption const& o, bool& out) { return out = o.getBool(o.user) != 0, true; }
bool setBool(ConfigOption const& o, bool v) { return o.setBool(o.user, v ? 1 : 0), true; }
bool getInt(ConfigOption const& o, int64_t& out) { return out = o.getInt(o.user), true; }
bool setInt(ConfigOption const& o, int64_t v) { return o.setInt(o.user, v), true; }
bool getFloat(ConfigOption const& o, double& out) { return out = o.getFloat(o.user), true; }
bool setFloat(ConfigOption const& o, double v) { return o.setFloat(o.user, v), true; }
bool getString(ConfigOption const& o, char* out, uint32_t capacity) { return o.getString(o.user, out, capacity), true; }
bool setString(ConfigOption const& o, char const* v) { return o.setString(o.user, v), true; }
bool action(ModMenuActionFn fn, void* user) {
    if (fn) fn(user);
    return true;
}
} // namespace modmenu::ui::safe
