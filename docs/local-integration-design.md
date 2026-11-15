# Local integration without Workspace

User scope confirmed 2026-11-16: integrate locally, exclude our Workspace implementation, plan other plugin migrations, do not submit upstream. No game installation or provider modification is authorized by this build task.

Use AFAlimru/ModMenuBE c80be256019543eac8852d1caf49f0e9553fd4b9 as the only menu/render/input shell. Import passive settings and exact native entry evidence from XuanDiNa/ModMenuBedrock 10f80f266523d3196e1af1f212375abadfa7c9a1. Preserve both MIT notices.

## Deliverable

Windows x64 local candidate `0.2.0-local1`, a DLL/package with checksums, manual testing instructions, and an evidence-based plugin migration table. No Workspace broker, controls, protocol exports or example provider. Existing external-screen API remains available to plugins that own a UI.

## Components

- Passive settings v1-v5 parser, validation, exact decimal slider and transactional Apply/Cancel remain compatible. File work is executed by a dedicated settings worker. Render calls edit a draft or enqueue load/apply; no file operations are introduced in this path.
- Loaded-only plugin list for runtime controls. Disabled/unloaded manifests are not advertised as live configuration providers. Existing upstream API v1/v2 remains ABI-compatible.
- Native bridge uses existing per-image verification and entry guards, never signatures guessed from neighboring versions. It only observes admitted main/pause scenes, translates resource-pack button events into queued menu requests and inserts optional default entry definitions. ImGui is the only renderer. No old native renderer or pointer hook is imported.
- Resource-pack event and ownership marker retain existing names. Exact native bridge refusal is explicit and never grants access to unknown scenes. TestHost has a separate compile-time host adapter and is not an exception in the game DLL.
- Foreground loss cancels active presses, sliders, text composition and capture; valid navigation/drafts remain. Resume requires the same eligible origin. Scene changes close stale UI. No automatic Apply or action is generated.
- Keyboard captures use the public Ctrl/Shift/Left Alt contract. Escape while capturing means NONE; ordinary Escape returns. Unsupported legacy plugin parsers are not silently upgraded to support chords.
- Game language is read through the verified native profile for compatibility field translations; unsupported language falls back to upstream language. Do not claim an untranslated upstream string dictionary supports Simplified Chinese.

## Acceptance

Build Debug and Release; run passive settings/transaction, binding, slider, resource entry and admission/focus tests. Validate exported API, missing Workspace exports, package whitelists, DLL hash and byte size. Gameplay remains pending: main/pause button, pack entry, settings Cancel/Apply, chord/unbound, focus/return, plugin co-existence. One target game's pass does not prove other versions. Do not modify a provider to bypass its old menu admission.
