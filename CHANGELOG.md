# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
