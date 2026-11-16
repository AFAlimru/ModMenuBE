# Local Non-Workspace Integration Plan

> Execute in this conversation using executing-plans. User has selected local implementation and explicitly excluded Workspace; no upstream submissions or other plugin modifications.

**Goal:** Produce a local test candidate combining ModMenuBE UI with compatible passive configuration and the resource-pack entry.

**Architecture:** Keep one upstream overlay. Import the passive model separately from the old menu core, put storage operations on a worker, and route validated native entry events into the upstream request queue.

**Tech Stack:** C++20, xmake/MSVC, ImGui 1.91.9, MinHook, nlohmann/json, Win32/DXGI.

**Spec:** `docs/local-integration-design.md`.

## Global Constraints

- No Workspace sources/exports/provider or provider changes.
- No installation, game launch, EXE modifications, GitHub submissions or weakened identity checks.
- Preserve MIT provenance and exact existing decimal/binding/transaction semantics.

## Tasks

- [x] Establish upstream Release baseline (`xmake build -y ModMenuBE`) before source changes.
- [x] Add failing integration tests for a v5 binding + slider descriptor, transactional Cancel/Apply, stale-file rejection and focus loss/resume. Run tests before implementing adapters.
- [x] Import only `Settings`, `SettingsInput`, `SettingsSchema`, `SettingsDescription`, `Slider`, `TextEditor` models and public binding header. Remove dependencies on old Core/Workspace; use narrowly extracted UTF8 helpers. Retest imported behavior.
- [x] Add `compat/SettingsService` for bounded worker queue, published draft results and cached loaded-plugin availability. Wire `ui/LegacySettings` to existing Configure/Back/footer; render never reads/writes settings. Test stale completion and explicit save behavior.
- [x] Add `compat/NativeBridge` with unchanged per-image verifier evidence. Observe scene/locale, handle resource-pack event, preserve default insertion/ownership. Install only reviewed required hooks; reject unknown owners. Test frame admission policy and definition transforms.
- [x] Preserve keyboard capture/unbound behavior and focus recovery in the single overlay. Run regressions against both Debug/Release.
- [x] Inventory only manifest + descriptor + config capability metadata of our plugins (no credentials/log payload). Write migration plan by dependency: descriptors only, public C API, old menu identity, external UI. Do not modify providers.
- [x] Package frozen candidate, run export/hash/size audit, and deliver manual test procedure. Keep accepted installed DLLs untouched.

Final verification and frozen packaging are executed using tools/verify_local.ps1 and tools/package_local.py. The supplied TestHost screenshot confirms the list only; manual gameplay remains pending.
