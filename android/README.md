# ModMenuBE for Android (native `.so` mod)

A LeviLauncher Android `preload-native` mod. It hooks `eglSwapBuffers` and draws the same OreUI-styled menu as
the Windows build (ImGui, OpenGL ES 3) on top of the game. Touch, Back and IME text come from the preloader input
API and are kept from the game while the menu is open.

## Shared code

The menu itself (`../src/core`, `../src/ui`, `../src/api`) is shared with Windows. Android supplies the platform
layer in `src/platform/`:

| Windows | Android |
| --- | --- |
| DXGI/D3D11 overlay, WndProc input | `EglOverlay.cpp` (eglSwapBuffers + ImGui GLES3), `TouchInput.cpp` (pl::input) |
| WinHTTP | `AndroidHttp.cpp` (java.net.HttpURLConnection via JNI) |
| WIC icons | `AndroidIconCache.cpp` (stb_image + GL textures) |
| `manifest.json.close` to disable | `mods/mods_config.json`, the launcher's own list (takes effect next start) |
| `GetModuleHandle` for "loaded" | `/proc/self/maps` |
| ShellExecute links | `Intent.ACTION_VIEW` |

Not on Android: opening another mod's menu by hotkey, "open folder" buttons, the hotkey setting.

## Build (Windows)

Needs the Android SDK with NDK 28.2 and CMake 3.22.1 (from the SDK manager).

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

Output: `build-arm64-v8a\ModMenuBE-<version>-arm64-v8a.levipack`. Import it in LeviLauncher.

## Use

- Tap the **MOD** floating button: a pixel button next to the game's emote/chat/pause buttons, top centre
  (LeviLauncher Mod Menu → ModMenuBE must be on). Move, resize or fade it in LeviLauncher's HUD editor; ModMenuBE's
  settings have shortcuts to that editor and to reset the position. While the menu is open LeviLauncher's floating
  buttons are hidden.
- With a hardware keyboard: **F8** anywhere.
- **Back** leaves a sub page, then closes the menu.

Dependencies fetched by CMake: preloader-android (pinned to the commit LeviLauncher-Android 1.6.1 ships, since the
C++ structs must match the launcher's runtime), Dear ImGui 1.91.9, nlohmann/json, fmt, stb_image.

Chinese text uses a CJK font from `/system/fonts`; fonts the ImGui rasteriser cannot parse (some phones ship
variable CJK fonts) are skipped, and the built-in font is the last resort.
