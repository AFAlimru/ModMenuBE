# ModMenuBE for Android (native `.so` mod) — preview

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

- In a world: tap the **MOD** floating button (LeviLauncher Mod Menu → ModMenuBE must be on).
- With a hardware keyboard: **F8** anywhere.
- **Back** leaves a sub page, then closes the menu.

Dependencies fetched by CMake: preloader-android 0.2.3, Dear ImGui 1.91.9, nlohmann/json, fmt, stb_image.
IME text needs a launcher runtime with preloader 0.2.3 or newer; on older ones the mod still loads.
