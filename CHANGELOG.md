# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.0-local3] - 2026-11-16 (local only)

### Fixed

- Passive settings: Apply and the discard explanation no longer share the same drawing area with the menu hotkey/status hint. Reserve a separate lower line, a consistent action height and the Cancel button's horizontal space.
- Use the active menu language for Apply, Cancel and the discard explanation. Long explanations are clipped to their own region.

This is a footer-only candidate based on local2. Existing settings transactions, provider APIs, native guards and the D3D12 loader fix are retained. No automatic installation or upstream submission.

## [0.2.0-local2] - 2026-11-16 (local only)

### Fixed

- Removed the eager SDK ordinal-101 import of `D3D12CreateDevice`, which prevented local1 from loading with the app-local `d3d12.dll` found in the test instance. The overlay now resolves the function by name from the absolute Windows system directory. No game DLL is replaced and no loader-anchor export is added.
- Added a binary dependency audit and standalone system-runtime resolution tests, including an already-mapped app-local shadow DLL. TestHost rendering alone did not cover this failure.

### Scope

- Retains the local1 passive settings/native entry integration, exact native guards and unchanged upstream C API. Workspace and other plugin updates remain excluded. Not published or submitted upstream; Minecraft testing is pending.

## [0.2.0] - 2026-11-13

### Added

- Android build for LeviLauncher-Android (`ModMenuBE-0.2.0-arm64-v8a.levipack`): the same menu, opened from a MOD button styled like the game's touch buttons at the top centre. Move, resize or fade it in LeviLauncher's HUD editor; the settings can open that editor or reset the button. LeviLauncher's floating buttons are hidden while the menu is open.
- Command buttons on a mod's page. Commands a mod registers with LeviLamina are found automatically (the button types the command into the chat for you to finish); `modmenu.json` `commands` and the new `addCommand` (C API version 2) add exact commands that are sent directly. Windows only.
- A Playback adapter: start, pause and stop recording from ModMenuBE.
- A logo (`icon.png`) for the mod list, LeviMC Mods and launchers.
- A dialog after enabling or disabling a mod saying that Minecraft has to be restarted.

### Fixed

- Opening the menu no longer crashes when the system's Chinese font cannot be read (seen on Samsung phones); other fonts are tried, then the built-in one.

## [0.1.1] - 2026-11-13

### Added

- `lip.toml`, so ModMenuBE is listed on LeviMC Mods and can be installed from LeviLauncher's mod browser.
- GitHub Actions that build the mod and attach `ModMenuBE-client-windows-x64.zip` to each release.

### Fixed

- Chinese, Japanese and Korean input methods can type into the search box and settings fields.
- Characters outside the preloaded font set (typed text, mod names and descriptions) are added to the font as they appear instead of showing as boxes.

## [0.1.0] - 2026-11-10

### Added

- First release: in-game mod list (F8), search, details with README, enable/disable, settings screens (`modmenu.json`, C API, generic JSON editor) and update checks.

[Unreleased]: https://github.com/AFAlimru/ModMenuBE/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/AFAlimru/ModMenuBE/compare/v0.1.1...v0.2.0
[0.1.1]: https://github.com/AFAlimru/ModMenuBE/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/AFAlimru/ModMenuBE/releases/tag/v0.1.0
