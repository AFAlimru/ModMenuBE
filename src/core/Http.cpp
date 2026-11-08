#include "core/Http.h"

#include "core/Util.h"

#include <Windows.h>
#include <winhttp.h>

namespace modmenu {

HttpResponse httpGet(std::string const& url, std::wstring const& extraHeaders, size_t maxBytes) {
    HttpResponse res;
    std::wstring wurl = fromUtf8(url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256]{};
    wchar_t path[2048]{};
    parts.lpszHostName     = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath      = path;
    parts.dwUrlPathLength  = 2048;
    wchar_t extra[2048]{};
    parts.lpszExtraInfo     = extra;
    parts.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts)) {
        res.error = "bad url";
        return res;
    }
    if (parts.nScheme != INTERNET_SCHEME_HTTPS) {
        res.error = "only https is allowed";
        return res;
    }
    std::wstring fullPath = std::wstring(path, parts.dwUrlPathLength) + std::wstring(extra, parts.dwExtraInfoLength);

    HINTERNET session = WinHttpOpen(L"ModMenuBE/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                    WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        res.error = "WinHttpOpen failed";
        return res;
    }
    WinHttpSetTimeouts(session, 5000, 5000, 8000, 8000);
    HINTERNET connect = WinHttpConnect(session, std::wstring(host, parts.dwHostNameLength).c_str(), parts.nPort, 0);
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", fullPath.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                : nullptr;
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    if (request) WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect, sizeof(redirect));
    std::wstring headers = L"Accept: application/json, text/plain\r\n" + extraHeaders;
    bool ok = request
           && WinHttpSendRequest(request, headers.c_str(), DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
           && WinHttpReceiveResponse(request, nullptr);
    if (ok) {
        DWORD size = sizeof(res.status);
        WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                            &res.status, &size, WINHTTP_NO_HEADER_INDEX);
        for (;;) {
            DWORD available = 0;
            if (!WinHttpQueryDataAvailable(request, &available) || available == 0) break;
            if (res.body.size() + available > maxBytes) {
                res.error = "response too large";
                break;
            }
            size_t old = res.body.size();
            res.body.resize(old + available);
            DWORD read = 0;
            if (!WinHttpReadData(request, res.body.data() + old, available, &read)) break;
            res.body.resize(old + read);
        }
    } else {
        res.error = "network error " + std::to_string(GetLastError());
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return res;
}

} // namespace modmenu
