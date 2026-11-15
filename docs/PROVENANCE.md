# Local provenance and review

Upstream UI shell: AFAlimru/ModMenuBE, commit `c80be256019543eac8852d1caf49f0e9553fd4b9` (MIT). Local integration/footer branches: `codex/local-nonworkspace` and `codex/footer-overlap`; consolidated review branch: `codex/modmenube-integration`. This candidate is proposed for upstream review; it is not an upstream release and has not been installed automatically.

Passive models and exact native evidence: XuanDiNa/ModMenuBedrock, commit `10f80f266523d3196e1af1f212375abadfa7c9a1`, `editions/vanilla` (MIT). Full original notice is retained in `integration/ModMenuBedrock-LICENSE.txt` and `THIRD_PARTY_NOTICES.txt`.

`NativeBridge.cpp` is a manually maintained narrow extraction of that commit's `NativeHost.cpp`: existing exact game profiles, menu observation/locale and definition transform. It excludes the old renderer, old pointer bridge, old menu Core and all Workspace code. Only the six reviewed definition/JSON/context/Down/Up/controller hooks are installed. No new RVA or game layout was inferred. Locale prerequisites and independent event-scene ownership checks are preserved. Shared Down/Up owners remain limited to the two audited MouseTweaks DLL hashes. The removed one-off import generator must not overwrite this reviewed adapter.

`Utf8.cpp` retains only the existing bounded validator from Storage; the old Storage/Core/Workspace implementation is not imported. `SettingsService`, `DraftInput`, `ReleaseGate`, `Admission`, and `LegacySettings` are local adapters. `Settings.h` uses the public binding header location. `json.hpp` forwards to the packaged nlohmann dependency.

The upstream public C API header is byte-equivalent after newline normalization. No Workspace export is added. `OverlayPreview.dll` replaces the bridge only in a separate development target, admits only TestHost.exe, and is excluded from the runtime ZIP. The game DLL never gains a test-host bypass. TestHost checks D3D rendering only; it does not prove Bedrock ABI behavior.

## Original import hashes

| Local file | Original file | Original SHA-256 | Exact bytes |
|---|---|---|---|
| `src/compat/Definition.cpp` | `src/Definition.cpp` | `373f48b7a9bcab6ac482258c27fe7c837e0312cfad6772e8f66585f8f66cac63` | yes |
| `src/compat/NativeCompatibility.inl` | `src/NativeCompatibility.inl` | `961aa16f63c07b6a516656f587f480ab14b5264cab7a9c0e1ef38cde00944117` | yes |
| `src/compat/NativeGuards.inl` | `src/NativeGuards.inl` | `0e7b971a14e50dd85e7bfa63cc27c13617f241bf48969fbb0899bd0354043cf6` | yes |
| `src/compat/NativeProfile.cpp` | `src/NativeProfile.cpp` | `854b0da5756becf5fbd4c3c5155669c96dbabc67a2887a889dbb6768886cfe87` | yes |
| `src/compat/Settings.cpp` | `src/Settings.cpp` | `0d3743ce60a98774762062e11d927bc041515ac73d1e775f4b197806273f6d30` | yes |
| `src/compat/SettingsDescription.cpp` | `src/SettingsDescription.cpp` | `134211cecd6b05a852b4f2321abb3f4e8d049ed43370031998128b0bf57549d7` | yes |
| `src/compat/SettingsInput.cpp` | `src/SettingsInput.cpp` | `c58bd08760b108976bb9e55be31ae2495675870abf4a5230944d99ff1c29c0e5` | yes |
| `src/compat/SettingsSchema.cpp` | `src/SettingsSchema.cpp` | `5ee4c107786987d2f83449622f378ba3f8cbb98ede9b2a7c0ab83b9f08a2562b` | yes |
| `src/compat/Slider.cpp` | `src/Slider.cpp` | `a54b053c3c05ee0c3f5ddebde8d37ecce2fc4139703682b495a79d1a0cb6e13c` | yes |
| `src/compat/TextEditor.cpp` | `src/TextEditor.cpp` | `eee879d3c917122261b803f4f765d9c8adb3802b1ae3e5146e3b92d8d992ee2a` | yes |
| `src/compat/ModMenu/Definition.h` | `include/ModMenu/Definition.h` | `3d520ae19a8b1416bef7e7cad0db410f3814981c7dceea1972c1b941ed52a231` | yes |
| `src/compat/ModMenu/NativeProfile.h` | `include/ModMenu/NativeProfile.h` | `c2ff8dfdc99eb7d10996b39a5a329f5e1f26d398a5aad57d4618309fc3f66ca7` | yes |
| `src/compat/ModMenu/Settings.h` | `include/ModMenu/Settings.h` | `5361af91bbd24d6046ce05c807de248eee5bc94337c035c56131a4f96ca5a8aa` | adapter changed |
| `src/compat/ModMenu/Slider.h` | `include/ModMenu/Slider.h` | `2b6f171fe8dd13965966e2234e3bcc676a0a4846667fbc9cbc18c85873007939` | yes |
| `src/compat/ModMenu/Storage.h` | `include/ModMenu/Storage.h` | `73261c1bc9ec9f4b046be8706733458cbd12c5db97f0706da7c6be2e7dffcc51` | adapter changed |
| `src/compat/ModMenu/TextEditor.h` | `include/ModMenu/TextEditor.h` | `308a68248ffc82998858edf74811b4c0b080285dd65f908891d2d635b0dfb6f1` | yes |

## Validation boundary

The local verification covers settings transactions, bounded async results, invalid input, decimal steps, modifier capture, NONE, focus/origin admission, held-input release, resource event identity and definition ownership, and exact PE/entry evidence. Native tests use private copy-on-write mappings for negative mutations; installed EXEs are never written.

The provided user TestHost screenshot confirms that the integrated list renders. Computer Use was stopped by the user with Escape; no further UI automation was performed. Gameplay acceptance for this new DLL remains pending on each game version. There is no claim that all existing binary plugins or resource-pack layouts are supported.
