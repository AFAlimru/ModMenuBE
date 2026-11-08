#pragma once

#include <string>

namespace modmenu {

struct HttpResponse {
    unsigned long status = 0;
    std::string   body;
    std::string   error;
};

// Blocking HTTPS GET (WinHTTP). Only https:// URLs are accepted. Call from a worker thread.
HttpResponse httpGet(std::string const& url, std::wstring const& extraHeaders = {}, size_t maxBytes = 4 * 1024 * 1024);

} // namespace modmenu
