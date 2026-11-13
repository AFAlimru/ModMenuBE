// ui/IconCache.h on Android: stb_image decodes on a worker thread, pump() uploads GL textures on the render thread.
#include "ui/IconCache.h"

#include "core/Log.h"
#include "core/Util.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace modmenu::ui::icons {

namespace {

struct Entry {
    enum class State { Decoding, Decoded, Ready, Failed } state = State::Decoding;
    std::vector<uint8_t> rgba;
    int                  width = 0, height = 0;
    GLuint               texture = 0;
    EGLContext           context = EGL_NO_CONTEXT; // the texture belongs to this GL context
};

std::mutex                   gMutex;
std::map<std::string, Entry> gEntries;

constexpr int kMaxSide = 128;

// Box-filter large icons down to at most kMaxSide (small pixel-art icons stay sharp).
void shrink(std::vector<uint8_t>& rgba, int& w, int& h) {
    if (w <= kMaxSide && h <= kMaxSide) return;
    int f  = (std::max(w, h) + kMaxSide - 1) / kMaxSide;
    int nw = std::max(1, w / f), nh = std::max(1, h / f);
    std::vector<uint8_t> out(size_t(nw) * nh * 4);
    for (int y = 0; y < nh; ++y)
        for (int x = 0; x < nw; ++x)
            for (int c = 0; c < 4; ++c) {
                unsigned sum = 0;
                for (int dy = 0; dy < f; ++dy)
                    for (int dx = 0; dx < f; ++dx) sum += rgba[(size_t(y * f + dy) * w + (x * f + dx)) * 4 + c];
                out[(size_t(y) * nw + x) * 4 + c] = uint8_t(sum / unsigned(f * f));
            }
    rgba = std::move(out);
    w    = nw;
    h    = nh;
}

} // namespace

ImTextureID get(std::filesystem::path const& png) {
    if (png.empty()) return ImTextureID{};
    std::string key = png.string();
    std::lock_guard lock(gMutex);
    auto [it, inserted] = gEntries.try_emplace(key);
    if (inserted) {
        std::thread([key] {
            int  w = 0, h = 0, n = 0;
            auto* data = stbi_load(key.c_str(), &w, &h, &n, 4);
            std::vector<uint8_t> rgba;
            if (data) {
                rgba.assign(data, data + size_t(w) * h * 4);
                stbi_image_free(data);
                shrink(rgba, w, h);
            }
            std::lock_guard l(gMutex);
            auto found = gEntries.find(key);
            if (found == gEntries.end()) return;
            if (rgba.empty()) {
                found->second.state = Entry::State::Failed;
                log::warn("could not decode icon {}", key);
                return;
            }
            found->second.rgba   = std::move(rgba);
            found->second.width  = w;
            found->second.height = h;
            found->second.state  = Entry::State::Decoded;
        }).detach();
    }
    return it->second.state == Entry::State::Ready ? ImTextureID(intptr_t(it->second.texture)) : ImTextureID{};
}

void pump() {
    std::lock_guard lock(gMutex);
    EGLContext ctx = eglGetCurrentContext();
    for (auto& [_, e] : gEntries) {
        if (e.state != Entry::State::Decoded) continue;
        GLint prev = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
        glGenTextures(1, &e.texture);
        glBindTexture(GL_TEXTURE_2D, e.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, e.width <= 32 ? GL_NEAREST : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, e.width, e.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, e.rgba.data());
        glBindTexture(GL_TEXTURE_2D, GLuint(prev));
        e.context = ctx;
        e.state   = e.texture ? Entry::State::Ready : Entry::State::Failed;
        e.rgba.clear();
        e.rgba.shrink_to_fit();
    }
}

void clear() {
    std::lock_guard lock(gMutex);
    EGLContext ctx = eglGetCurrentContext();
    for (auto& [_, e] : gEntries) {
        // A texture of a dead context must not be deleted by name: the id may now be one of the game's.
        if (e.texture && e.context == ctx && ctx != EGL_NO_CONTEXT) glDeleteTextures(1, &e.texture);
    }
    gEntries.clear();
}

} // namespace modmenu::ui::icons
