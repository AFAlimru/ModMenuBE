// core/Http.h on Android: java.net.HttpURLConnection through JNI (the system handles TLS and proxies).
#include "core/Http.h"

#include "core/Util.h"
#include "platform/Jni.h"

#include <string>
#include <vector>

namespace modmenu {

namespace {

struct LocalFrame {
    JNIEnv* e;
    explicit LocalFrame(JNIEnv* env) : e(env) { e->PushLocalFrame(64); }
    ~LocalFrame() { e->PopLocalFrame(nullptr); }
};

// "Name: value\r\n" lines -> setRequestProperty calls.
void applyHeaders(JNIEnv* e, jobject conn, jmethodID setProp, std::string const& headers) {
    size_t pos = 0;
    while (pos < headers.size()) {
        size_t end  = headers.find("\r\n", pos);
        auto   line = headers.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        pos         = end == std::string::npos ? headers.size() : end + 2;
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        auto name  = line.substr(0, colon);
        auto value = line.substr(colon + 1);
        while (!value.empty() && value.front() == ' ') value.erase(0, 1);
        jstring jn = e->NewStringUTF(name.c_str());
        jstring jv = e->NewStringUTF(value.c_str());
        e->CallVoidMethod(conn, setProp, jn, jv);
        e->DeleteLocalRef(jn);
        e->DeleteLocalRef(jv);
    }
}

} // namespace

HttpResponse httpGet(std::string const& url, std::wstring const& extraHeaders, size_t maxBytes) {
    HttpResponse res;
    if (toLower(url).rfind("https://", 0) != 0) {
        res.error = "only https is allowed";
        return res;
    }
    JNIEnv* e = android::jni::env();
    if (!e) {
        res.error = "no JNI environment";
        return res;
    }
    LocalFrame frame(e);
    auto fail = [&](char const* what) {
        auto ex   = android::jni::takeException(e);
        res.error = std::string(what) + (ex.empty() ? "" : ": " + ex);
        return res;
    };

    jclass    urlCls   = e->FindClass("java/net/URL");
    jmethodID urlCtor  = e->GetMethodID(urlCls, "<init>", "(Ljava/lang/String;)V");
    jmethodID openConn = e->GetMethodID(urlCls, "openConnection", "()Ljava/net/URLConnection;");
    jstring   jurl     = e->NewStringUTF(url.c_str());
    jobject   urlObj   = e->NewObject(urlCls, urlCtor, jurl);
    if (e->ExceptionCheck() || !urlObj) return fail("bad url");
    jobject conn = e->CallObjectMethod(urlObj, openConn);
    if (e->ExceptionCheck() || !conn) return fail("openConnection failed");

    jclass    httpCls     = e->FindClass("java/net/HttpURLConnection");
    jmethodID setConnTo   = e->GetMethodID(httpCls, "setConnectTimeout", "(I)V");
    jmethodID setReadTo   = e->GetMethodID(httpCls, "setReadTimeout", "(I)V");
    jmethodID setProp     = e->GetMethodID(httpCls, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
    jmethodID getCode     = e->GetMethodID(httpCls, "getResponseCode", "()I");
    jmethodID getInput    = e->GetMethodID(httpCls, "getInputStream", "()Ljava/io/InputStream;");
    jmethodID getError    = e->GetMethodID(httpCls, "getErrorStream", "()Ljava/io/InputStream;");
    jmethodID disconnect  = e->GetMethodID(httpCls, "disconnect", "()V");
    e->CallVoidMethod(conn, setConnTo, 5000);
    e->CallVoidMethod(conn, setReadTo, 8000);
    applyHeaders(e, conn, setProp, "User-Agent: ModMenuBE/0.2 (Android)\r\n" + toUtf8(extraHeaders));
    if (e->ExceptionCheck()) return fail("request setup failed");

    jint code = e->CallIntMethod(conn, getCode);
    if (e->ExceptionCheck()) {
        e->CallVoidMethod(conn, disconnect);
        return fail("request failed");
    }
    res.status = static_cast<unsigned long>(code);
    jobject in = e->CallObjectMethod(conn, code >= 400 ? getError : getInput);
    if (e->ExceptionCheck()) android::jni::takeException(e), in = nullptr;

    if (in) {
        jclass     inCls = e->FindClass("java/io/InputStream");
        jmethodID  read  = e->GetMethodID(inCls, "read", "([B)I");
        jmethodID  close = e->GetMethodID(inCls, "close", "()V");
        jbyteArray buf   = e->NewByteArray(16384);
        std::vector<jbyte> chunk(16384);
        for (;;) {
            jint n = e->CallIntMethod(in, read, buf);
            if (e->ExceptionCheck()) {
                res.error = "read failed: " + android::jni::takeException(e);
                break;
            }
            if (n < 0) break;
            e->GetByteArrayRegion(buf, 0, n, chunk.data());
            res.body.append(reinterpret_cast<char const*>(chunk.data()), size_t(n));
            if (res.body.size() > maxBytes) {
                res.error = "response too large";
                break;
            }
        }
        e->CallVoidMethod(in, close);
        android::jni::takeException(e);
    }
    e->CallVoidMethod(conn, disconnect);
    android::jni::takeException(e);
    return res; // like the WinHTTP version, callers judge the status code themselves
}

} // namespace modmenu
