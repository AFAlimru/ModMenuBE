// Header-only C++ helper over ModMenuApi.h. No link dependency on ModMenuBE:
// everything is resolved with GetModuleHandle/GetProcAddress, so your mod
// still loads (and simply skips the config screen) when ModMenuBE is absent.
//
//   #include <modmenu/ModMenuApi.hpp>
//
//   void registerConfigScreen() {
//       auto screen = modmenu::Screen::begin("MyMod", "My Mod Settings");
//       if (!screen) return;                              // ModMenuBE not installed
//       screen.category("general", "General");
//       screen.toggle("enabled", "Enabled", [] { return gCfg.enabled.load(); },
//                     [](bool v) { gCfg.enabled = v; });
//       screen.slider("range", "Range", 1, 64, [] { return gCfg.range.load(); },
//                     [](int64_t v) { gCfg.range = int(v); });
//       screen.onSave([] { saveConfig(); });
//   }
//
// Callbacks run on the render thread; see ModMenuApi.h.
#pragma once

#include "ModMenuApi.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace modmenu {

inline ModMenuApi const* api() {
    static ModMenuApi const* cached = [] () -> ModMenuApi const* {
        HMODULE module = GetModuleHandleW(L"ModMenuBE.dll");
        if (!module) return nullptr;
        auto get = reinterpret_cast<ModMenu_GetApiFn>(GetProcAddress(module, "ModMenu_GetApi"));
        return get ? get(1) : nullptr; // newer entries check api()->version themselves
    }();
    return cached;
}

inline bool available() { return api() != nullptr; }

namespace detail {
// Callback holders are owned by the screen for the lifetime of the process;
// ModMenuBE keeps raw `user` pointers to them.
struct Holder {
    virtual ~Holder() = default;
};
template <class T>
struct Box : Holder {
    T value;
    explicit Box(T v) : value(std::move(v)) {}
};
inline std::vector<Holder*>& keepAlive() {
    static std::vector<Holder*> all;
    return all;
}
template <class T>
Box<T>* box(T value) {
    auto* b = new Box<T>(std::move(value));
    keepAlive().push_back(b);
    return b;
}
inline char const* cstr(std::string_view s, std::string& storage) {
    storage.assign(s);
    return storage.c_str();
}
} // namespace detail

class Screen {
public:
    static Screen begin(std::string_view mod, std::string_view title = {}) {
        Screen s;
        if (ModMenuApi const* a = api()) {
            s.mod_ = std::string(mod);
            std::string t(title);
            s.ok_ = a->beginScreen(s.mod_.c_str(), title.empty() ? nullptr : t.c_str()) != 0;
        }
        return s;
    }

    explicit operator bool() const { return ok_; }

    Screen& category(std::string_view id, std::string_view title) {
        if (ok_) api()->addCategory(mod_.c_str(), std::string(id).c_str(), std::string(title).c_str());
        return *this;
    }

    Screen& header(std::string_view text) {
        if (ok_) api()->addHeader(mod_.c_str(), std::string(text).c_str());
        return *this;
    }

    Screen& toggle(std::string_view id, std::string_view label, std::function<bool()> get,
                   std::function<void(bool)> set, std::string_view tooltip = {}) {
        if (!ok_) return *this;
        using Pair = std::pair<std::function<bool()>, std::function<void(bool)>>;
        auto* b = detail::box(Pair{std::move(get), std::move(set)});
        std::string tip(tooltip);
        api()->addBool(
            mod_.c_str(), std::string(id).c_str(), std::string(label).c_str(), tip.empty() ? nullptr : tip.c_str(),
            [](void* u) -> int { return static_cast<detail::Box<Pair>*>(u)->value.first() ? 1 : 0; },
            [](void* u, int v) { static_cast<detail::Box<Pair>*>(u)->value.second(v != 0); }, b
        );
        return *this;
    }

    Screen& slider(std::string_view id, std::string_view label, int64_t min, int64_t max,
                   std::function<int64_t()> get, std::function<void(int64_t)> set, std::string_view tooltip = {}) {
        if (!ok_) return *this;
        using Pair = std::pair<std::function<int64_t()>, std::function<void(int64_t)>>;
        auto* b = detail::box(Pair{std::move(get), std::move(set)});
        std::string tip(tooltip);
        api()->addInt(
            mod_.c_str(), std::string(id).c_str(), std::string(label).c_str(), tip.empty() ? nullptr : tip.c_str(),
            min, max, [](void* u) -> int64_t { return static_cast<detail::Box<Pair>*>(u)->value.first(); },
            [](void* u, int64_t v) { static_cast<detail::Box<Pair>*>(u)->value.second(v); }, b
        );
        return *this;
    }

    Screen& sliderF(std::string_view id, std::string_view label, double min, double max, double step,
                    std::function<double()> get, std::function<void(double)> set, char const* format = nullptr,
                    std::string_view tooltip = {}) {
        if (!ok_) return *this;
        using Pair = std::pair<std::function<double()>, std::function<void(double)>>;
        auto* b = detail::box(Pair{std::move(get), std::move(set)});
        std::string tip(tooltip);
        api()->addFloat(
            mod_.c_str(), std::string(id).c_str(), std::string(label).c_str(), tip.empty() ? nullptr : tip.c_str(),
            min, max, step, format, [](void* u) -> double { return static_cast<detail::Box<Pair>*>(u)->value.first(); },
            [](void* u, double v) { static_cast<detail::Box<Pair>*>(u)->value.second(v); }, b
        );
        return *this;
    }

    Screen& text(std::string_view id, std::string_view label, uint32_t maxBytes, std::function<std::string()> get,
                 std::function<void(std::string const&)> set, std::string_view tooltip = {}) {
        if (!ok_) return *this;
        using Pair = std::pair<std::function<std::string()>, std::function<void(std::string const&)>>;
        auto* b = detail::box(Pair{std::move(get), std::move(set)});
        std::string tip(tooltip);
        api()->addString(
            mod_.c_str(), std::string(id).c_str(), std::string(label).c_str(), tip.empty() ? nullptr : tip.c_str(),
            maxBytes,
            [](void* u, char* out, uint32_t cap) {
                if (cap == 0) return;
                std::string v = static_cast<detail::Box<Pair>*>(u)->value.first();
                size_t n = v.size() < cap - 1 ? v.size() : cap - 1;
                memcpy(out, v.data(), n);
                out[n] = 0;
            },
            [](void* u, char const* v) { static_cast<detail::Box<Pair>*>(u)->value.second(v ? v : ""); }, b
        );
        return *this;
    }

    Screen& choice(std::string_view id, std::string_view label, std::vector<std::string> const& choices,
                   std::function<int64_t()> get, std::function<void(int64_t)> set, std::string_view tooltip = {}) {
        if (!ok_) return *this;
        std::string packed;
        for (auto const& c : choices) {
            packed += c;
            packed.push_back('\0');
        }
        packed.push_back('\0');
        using Pair = std::pair<std::function<int64_t()>, std::function<void(int64_t)>>;
        auto* b = detail::box(Pair{std::move(get), std::move(set)});
        std::string tip(tooltip);
        api()->addEnum(
            mod_.c_str(), std::string(id).c_str(), std::string(label).c_str(), tip.empty() ? nullptr : tip.c_str(),
            packed.c_str(), [](void* u) -> int64_t { return static_cast<detail::Box<Pair>*>(u)->value.first(); },
            [](void* u, int64_t v) { static_cast<detail::Box<Pair>*>(u)->value.second(v); }, b
        );
        return *this;
    }

    Screen& button(std::string_view id, std::string_view label, std::function<void()> onClick,
                   std::string_view tooltip = {}) {
        if (!ok_) return *this;
        auto* b = detail::box(std::move(onClick));
        std::string tip(tooltip);
        api()->addButton(
            mod_.c_str(), std::string(id).c_str(), std::string(label).c_str(), tip.empty() ? nullptr : tip.c_str(),
            [](void* u) { static_cast<detail::Box<std::function<void()>>*>(u)->value(); }, b
        );
        return *this;
    }

    Screen& onSave(std::function<void()> fn) {
        if (!ok_) return *this;
        auto* b = detail::box(std::move(fn));
        api()->setOnSave(mod_.c_str(), [](void* u) { static_cast<detail::Box<std::function<void()>>*>(u)->value(); }, b);
        return *this;
    }

private:
    std::string mod_;
    bool        ok_ = false;
};

// For mods that already ship their own settings UI.
inline bool setExternalScreen(std::string_view mod, std::function<void()> open) {
    ModMenuApi const* a = api();
    if (!a) return false;
    auto* b = detail::box(std::move(open));
    return a->setExternalScreen(
               std::string(mod).c_str(),
               [](void* u) { static_cast<detail::Box<std::function<void()>>*>(u)->value(); }, b
           ) != 0;
}

// A command button on the mod's details page (needs ModMenuBE with API version 2).
inline bool addCommand(std::string_view mod, std::string_view command, std::string_view label = {},
                       std::string_view description = {}) {
    ModMenuApi const* a = api();
    if (!a || a->version < 2) return false;
    std::string l(label), d(description);
    return a->addCommand(std::string(mod).c_str(), std::string(command).c_str(), l.empty() ? nullptr : l.c_str(),
                         d.empty() ? nullptr : d.c_str()) != 0;
}

inline bool setMetadata(std::string_view mod, std::string_view json) {
    ModMenuApi const* a = api();
    return a && a->setMetadata(std::string(mod).c_str(), std::string(json).c_str()) != 0;
}

} // namespace modmenu
