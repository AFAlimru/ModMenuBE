# Optional Workspace Extension Implementation Plan

> Execute inline using executing-plans under the user's existing authorization; no subagents, installation, game control or remote updates.

**Goal:** Deliver a Windows local test build with opt-in modmenu.json entry routing, embedded OreUI workspace controls and explicit external-UI handoff.

**Architecture:** Keep the upstream API v1/v2 untouched. Add a bounded versioned copy-buffer workspace API, reuse the licensed strict legacy protocol/parser/broker, and add platform ownership checks plus an ImGui view/controller. Runtime state is independent from presentation; a worker polls explicit requests, never a render callback.

**Tech Stack:** C++20, nlohmann/json, ImGui/OreTheme, Win32 identity boundary, xmake, Python package audit.

**Spec:** `docs/superpowers/specs/2026-11-16-workspace-extension-design.md`.

## Global constraints

- New local version; preserve frozen local3 and PR #2. Use current isolated worktree, branch codex/workspace-extension.
- No changes to game native profiles/hooks, other plugins, installed settings or credentials.
- Workspace absent/false hides entry; valid declaration shows entry, unavailable targets remain disabled. Configure/open stay independent.
- Seven bounded controls, max 16 providers, 8 pages, 64 controls, 16 entries, one 64x48 image, 16 pending commands; 64KiB declaration/256KiB state/32KiB command including NUL.
- Explicit user operations only; no jobs on open/refresh/close/focus, no retries. External handoff waits for input release and has a bounded acknowledgement timeout.
- Old API header unchanged, one new versioned query export, no legacy DLL impersonation. Provider adaptation remains separate.

## Task 1: Opt-in declaration and routing

Files: new `src/workspace/Entry.h/.cpp`, `tests/WorkspaceEntryTests.cpp`; modify Adapter, schema, xmake.

- [x] Write failing tests: `parseEntry(object)` accepts absent/false with visible=false; validates page/external, translated labels, IDs/count/version, rejects duplicate/unknown/unsafe targets. Use fixture JSON from the spec.
- [x] Run `xmake build WorkspaceEntryTests` and executable, observe missing capability/failure.
- [x] Implement `EntryDefinition parseEntry(json const&)` with `declared`, `valid`, localized label, typed targets and bounded error. Adapter parses only its workspace member and excludes it from metadata.
- [x] Add editor schema; run actual parser fixtures and legacy metadata/config regression, commit.

## Task 2: Workspace data model and explicit handoff

Files: import licensed `src/workspace/Protocol.h/.cpp`; new `Broker.h/.cpp`, `tests/WorkspaceProtocolTests.cpp`, `tests/WorkspaceHandoffTests.cpp`.

- [x] Import legacy tests first and run missing-feature red case; preserve original hashes/MIT source commit 10f80f2.
- [x] Reuse strict declaration/state parser and broker; test `Register/Publish/Poll/Submit/Unregister`, short-buffer FIFO, bounded queues, stale revisions/session, passive snapshots, all control types and duplicate JSON.
- [x] Add external target registration plus `BeginHandoff`, `ReleaseHandoff`, `Acknowledge`, `RequestReturn`, `Expire`; commands are released only after render/input ownership is suspended and all held inputs are released. Enforce session/request ownership and timeout with a supplied monotonic clock in tests.
- [x] Run the real consumer tests; inspect malformed/failed states atomically retaining prior data; commit.

## Task 3: Versioned public API and identity

Files: new `include/modmenu/ModMenuWorkspace.h`, `src/workspace/Api.cpp`, `Runtime.h/.cpp`, `tests/WorkspaceApiTests.cpp`; modify xmake.

- [x] Write ABI size/version tests, owner mismatch, absent/disabled entry, no menu readiness, result-buffer bounds and expired handoff tests.
- [x] Export `ModMenu_GetWorkspaceApi(uint32_t)` with version+structSize and copied register/publish/poll/unregister/handoff/return calls. Use no retained foreign callback.
- [x] Runtime checks exact loaded module path/manifest name/version and normal sibling plugin folder, validates executable owner address, pins module; data core stays portable. Do all registration/file work on the provider worker.
- [x] Preserve `ModMenuApi.h` byte identity. Verify exactly two exports and old clients; commit.

## Task 4: Embedded UI, focus and navigation

Files: new `src/ui/WorkspaceView.h/.cpp`, `src/workspace/ViewModel.h/.cpp`; modify ModMenuScreen, Overlay, I18n.

- [x] Test view model navigation/session invalidation, committed drafts, readonly refresh, confirmation cancellation, explicit back, focus release and external return without auto-open.
- [x] Add workspace launcher view, button cards and tabs with existing OreTheme; render text/boolean/action/status/progress/stable lists/bounded pixels. Use clipped child windows and wrapped localized text.
- [x] Connect details entry visibility to Adapter declaration, target readiness to Runtime snapshot. Retain Configure and external-screen controls.
- [x] Add explicit external handoff/return flow to existing origin admission. Blur cancels transient ImGui state only; scene/session invalidation closes stale workspaces. Clear texture resources at overlay teardown; no new native hooks.
- [x] Run view-model tests and offscreen actual UI render cases in English/Chinese/narrow/scale, commit.

## Task 5: Demonstration and testable delivery

Files: new `integration/workspace-example`, `integration/WORKSPACES.md`, tests and packaging scripts; modify version, changelog, README/testing docs.

- [x] Build a separate opt-in demo DLL with all seven controls, two pages, two distinct external UI target buttons, worker acknowledgements and explicit return. Never install automatically.
- [x] Run Debug/Release existing suites plus new protocol/entry/handoff/API/view tests, public C header compile, original/launcher image checks and DLL dependency audit.
- [x] Package immutable runtime/source/demo ZIPs, source provenance and test logs; audit exports, versions, file allowlist, source rebuildability, CRC/SHA256 and DLL byte size.
- [x] Copy immutable delivery to project releases on D:, provide focused manual steps and clearly separate offline checks from gameplay. Leave PR #2 untouched and wait for user acceptance.

## Execution record

Implemented inline on `codex/workspace-extension`; no subagents or remote changes. Production workspace UI uses existing OreTheme. Bounded pixel rendering uses CPU-cached draw-list rectangles instead of retaining a texture; device cleanup clears the cache. The new `WorkspaceUiTests` runs actual renderer and pointer input on a windowless D3D11 WARP target. C11 header compilation is linked into the actual API suite. Debug/Release now run 14 executable suites plus dependency checks and 145 private image checks per mode.

The Service normalizes external-only declarations through a private temporary status page before the strict legacy parser, then removes it; this permits empty pages only with registered external targets. It does not expose an internal page or command. Other plugins remain unmodified. Packaging and immutable local delivery are recorded in the release audit, with manual gameplay explicitly pending.
