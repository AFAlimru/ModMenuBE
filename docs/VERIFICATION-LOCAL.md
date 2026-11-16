# Verification status: 0.2.0-local4

Local opt-in workspace candidate on reviewed base `5781ba0`. No installation, game launch, provider update or remote publication occurred. Frozen local3 and PR #2 remain unchanged.

- Debug and Release: 14 executable suites passed, plus the static loader dependency audit. The new suites cover entry opt-in, the actual strict protocol/broker, external handoff/session/timeouts, real Win32 module/manifest/descriptor ownership, view state, and the actual ImGui workspace renderer/input.
- NativeTests: 145 checks passed in each configuration against original and launcher forms of 1.26.45.01, 1.26.51.01, 1.26.52.03 and 1.26.60.05. App-local D3D12 shadow checks pass. These are offline copy-on-write checks, not Minecraft acceptance. NativeBridge/profile/guard sources are byte-equivalent to the reviewed base after newline normalization.
- The public workspace header compiles as C11 and shares the C++ table layout. Existing upstream `ModMenuApi.h` v1/v2 is unchanged. Runtime package exports exactly `ModMenu_GetApi` and `ModMenu_GetWorkspaceApi`, with no old Workspace aliases or test bypass DLLs.
- Actual offscreen UI tests use D3D11 WARP without a window or game: pointer clicks open an embedded page and submit the exact form; pending actions do not replay; passive render does not enqueue; physical-release handoff gating, explicit return, retained page/scroll and origin invalidation pass. Six English/Traditional Chinese, narrow and 150% scale images were inspected. The image preview has no retained GPU resources.
- The separate example DLL builds in both modes. Its worker and two own-window external targets are for manual acceptance; no example window has been opened in this verification. It performs no login, network or player-file operations. Acknowledgement is not task completion.
- Packaging validates fresh verification DLL hashes/byte sizes, PE exports/dependencies, source provenance, JSON, runtime allowlists and ZIP roundtrips/CRC. Main runtime and optional example are separate. SDK source includes schemas and reproducible offscreen tests.

Pending manual acceptance: all workspace controls/IME/clipboard, page and focus restoration, external own-window return/X/F10 behavior, main/pause navigation, Configure coexistence and resource-pack entry in Minecraft. Existing legacy Workspace providers need their own query/identity adaptation and testing; no BedrockTool DLL was changed or included.

Build environment: process-local `_CL_=/Z7` avoids the shared MSVC PDB RPC issue and is restored after verification. No shared developer process was stopped.

The following local3 record is historical.

# Verification status: 0.2.0-local3

This candidate changes only passive-settings footer layout and labels on the frozen local2 base. The action row and status/hotkey row have separate vertical space; explanatory text is clipped before the Cancel button. No provider/API/native-profile or companion-admission changes are included. Offscreen previews exercise the actual production footer with the existing fonts/theme; this is visual evidence, not Minecraft gameplay acceptance. The old footer reproduced the reported overlap. Six images were inspected: before, English/Traditional Chinese after, 500 px and 320 px widths, and 1.5 scale. The action/status rows are separated and narrow text stops before Cancel. A D3D11 WARP texture was read back without opening a window or starting Minecraft.

- Debug/Release build and eight executable suites: passed (Definition 38, native synthetic 52, 26.51 identity 6, passive external settings 93; transaction cleanup/text editor/local integration report zero failures; D3D12RuntimeTests verifies system function resolution).
- Four local game versions, original and launcher EXE forms: 145 checks passed in both Debug and Release using NativeTests. Mutation checks use private copy-on-write memory.
- Eight existing sidecars: AutoClick, AutoJumpReset, ContainerPreview, FakeSneak, FullBright, ToggleSprint, SodiumExtraBedrock, LeftHandView parsed by the imported actual consumer.
- Worker tests cover bounded outstanding results, discard capacity, stale completion rejection and an error result with an empty plugin ID. Draft input tests cover invalid values and intentional empty editing. Capture tests cover Ctrl+Shift+LAlt+K, missing modifiers, Esc NONE and rejected mouse primaries. Focus tests retain origin on temporary loss, invalidate changed origin and wait for held keys/buttons to release.
- Resource entry tests cover exact event identity, unrelated events, transform ownership/idempotence, duplicate fields and size bounds. Private vanilla menu payload fixtures were not supplied to DefinitionTests; it exercised synthetic layouts. Runtime rendering of resource-pack buttons remains for user testing.
- Public C API header matches upstream; package tool enforces x64 and exactly one export, ModMenu_GetApi. Workspace exports and OverlayPreview.dll are absent from the runtime ZIP.
- The frozen local1 DLL reproduces the binary dependency regression: it requests `d3d12.dll` ordinal 101, absent in the test instance's app-local DLL. local2 has no eager `d3d12.dll` import. Both configurations resolve the actual system `D3D12CreateDevice` by name, including with the app-local DLL already mapped without execution. This does not start Minecraft or exercise the renderer. See LOADER-DEPENDENCY.md.
- TestHost log reached D3D12/D3D11On12 renderer/font initialization; the user supplied a screenshot of the list. Computer Use was stopped using the physical Escape key, and no further UI automation occurred. This is not Minecraft gameplay verification.
- During local verification, no game EXE, installed plugin/config, provider source, GitHub issue/PR or remote branch was changed. Other plugin source fingerprints were checked against the inventory (11 files). Publication for upstream review is a separate later step and does not imply Minecraft acceptance.

Pending manual acceptance: native main/pause entry on each version, resource-pack entry, actual provider reload/Cancel, real key capture, focus return and audited MouseTweaks coexistence. BedrockTool workspace is excluded and handled by the user's other branch conversation.

Build logs are retained locally under artifacts/verification; public source ZIP contains only allowlisted source, docs, SDK fixtures and assets.

Build environment note (local3): the initial MSVC /Zi attempts hit C1090, PDB API RPC error 0x6BA. Debug verification was retried with the process-local `_CL_=/Z7` option to embed compiler debug information rather than contact the shared PDB service. The variable was restored afterwards; no shared tool/process was stopped. Release DLL was built with the unchanged project flags.
