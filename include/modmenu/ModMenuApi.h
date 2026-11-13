/*
 * ModMenuBE public C API (v1)
 *
 * Other mods use this to give ModMenuBE a config screen, the way Java mods
 * implement ModMenuApi#getModConfigScreenFactory. The API is a plain C ABI so
 * a mod never has to link against ModMenuBE: look the functions up at runtime
 * (see ModMenuApi.hpp for a header-only C++ helper that does it for you).
 *
 *   HMODULE mm = GetModuleHandleW(L"ModMenuBE.dll");
 *   auto getApi = (ModMenu_GetApiFn)GetProcAddress(mm, "ModMenu_GetApi");
 *   ModMenuApi const* api = getApi ? getApi(MODMENU_API_VERSION) : NULL;
 *
 * Rules
 *  - `mod` is the "name" field of your manifest.json, so ModMenuBE can attach
 *    the screen to the right entry in the mod list.
 *  - All strings are UTF-8. ModMenuBE copies every string it is given.
 *  - Registration functions may be called from any thread.
 *  - get/set/click callbacks are called on the game's RENDER thread while the
 *    menu is open. Keep them cheap and thread-safe (an atomic or a mutex).
 *  - `user` is passed back to your callbacks untouched.
 *  - Functions return 1 on success and 0 on failure (unknown category,
 *    duplicate id, bad arguments).
 */
#ifndef MODMENU_API_H
#define MODMENU_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MODMENU_API_VERSION 2u

typedef int (*ModMenuGetBoolFn)(void* user);
typedef void (*ModMenuSetBoolFn)(void* user, int value);
typedef int64_t (*ModMenuGetIntFn)(void* user);
typedef void (*ModMenuSetIntFn)(void* user, int64_t value);
typedef double (*ModMenuGetFloatFn)(void* user);
typedef void (*ModMenuSetFloatFn)(void* user, double value);
/* Write at most `capacity` bytes (including the terminating 0) into `out`. */
typedef void (*ModMenuGetStringFn)(void* user, char* out, uint32_t capacity);
typedef void (*ModMenuSetStringFn)(void* user, char const* value);
typedef void (*ModMenuActionFn)(void* user);

typedef struct ModMenuApi {
    uint32_t version;     /* MODMENU_API_VERSION the table was built for   */
    uint32_t structSize;  /* sizeof(ModMenuApi) on the ModMenuBE side       */

    /* Starts (or replaces) the config screen of `mod`. `title` may be NULL. */
    int (*beginScreen)(char const* mod, char const* title);

    /* Adds a tab. Options added after this go into it until the next call.
     * Without any category, options go to a default "General" tab. */
    int (*addCategory)(char const* mod, char const* id, char const* title);

    /* A bold caption line inside the current category. */
    int (*addHeader)(char const* mod, char const* text);

    /* tooltip may be NULL. */
    int (*addBool)(char const* mod, char const* id, char const* label, char const* tooltip,
                   ModMenuGetBoolFn get, ModMenuSetBoolFn set, void* user);

    int (*addInt)(char const* mod, char const* id, char const* label, char const* tooltip,
                  int64_t min, int64_t max, ModMenuGetIntFn get, ModMenuSetIntFn set, void* user);

    /* step <= 0 means a free slider; format is printf-style, NULL = "%.2f". */
    int (*addFloat)(char const* mod, char const* id, char const* label, char const* tooltip,
                    double min, double max, double step, char const* format,
                    ModMenuGetFloatFn get, ModMenuSetFloatFn set, void* user);

    int (*addString)(char const* mod, char const* id, char const* label, char const* tooltip,
                     uint32_t maxBytes, ModMenuGetStringFn get, ModMenuSetStringFn set, void* user);

    /* `choices` is "\0"-separated and ends with "\0\0", e.g. "Low\0Mid\0High\0\0".
     * The int getter/setter use the choice index. */
    int (*addEnum)(char const* mod, char const* id, char const* label, char const* tooltip,
                   char const* choices, ModMenuGetIntFn get, ModMenuSetIntFn set, void* user);

    /* A button inside the screen (e.g. "Reset to defaults"). */
    int (*addButton)(char const* mod, char const* id, char const* label, char const* tooltip,
                     ModMenuActionFn onClick, void* user);

    /* Called when the screen is closed or the user presses "Save". */
    int (*setOnSave)(char const* mod, ModMenuActionFn onSave, void* user);

    /* For mods that already have their own UI: the "Config" button closes
     * ModMenuBE and calls `open` instead of showing a ModMenuBE screen. */
    int (*setExternalScreen)(char const* mod, ModMenuActionFn open, void* user);

    /* Extra metadata merged over manifest.json at runtime. JSON object with
     * the same keys as the manifest's "modmenu" block (description, icon,
     * links, badges, parent, license, update, ...). */
    int (*setMetadata)(char const* mod, char const* jsonObject);

    /* Removes everything `mod` registered (call it when your mod unloads). */
    int (*unregister)(char const* mod);

    /* Opens or closes the menu. `mod` may be NULL; otherwise that mod is
     * selected (and its config screen opened when openConfig != 0). */
    int (*openMenu)(char const* mod, int openConfig);
    int (*closeMenu)(void);
    int (*isMenuOpen)(void);

    /* ---- version 2 ---- check `version >= 2` before using these. */

    /* Adds a command button to the mod's details page. `command` is typed
     * into the chat without the leading '/' (e.g. "record start") and sent.
     * `label` (button text) and `description` (tooltip) may be NULL.
     * Commands registered with LeviLamina are also found automatically;
     * use this for exact actions such as subcommands. */
    int (*addCommand)(char const* mod, char const* command, char const* label, char const* description);
} ModMenuApi;

/* Returns NULL if ModMenuBE is older than `minVersion`. */
typedef ModMenuApi const* (*ModMenu_GetApiFn)(uint32_t minVersion);

#if defined(_WIN32)
#define MODMENU_EXPORT __declspec(dllexport)
#else
#define MODMENU_EXPORT __attribute__((visibility("default")))
#endif

#if defined(MODMENU_BUILDING)
MODMENU_EXPORT ModMenuApi const* ModMenu_GetApi(uint32_t minVersion);
#endif

#ifdef __cplusplus
}
#endif

#endif /* MODMENU_API_H */
