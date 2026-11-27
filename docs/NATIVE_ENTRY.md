# Optional Windows native entry

The general overlay, F8 shortcut, existing input guard and settings screens do
not depend on this extension. Exact native admission runs on a separate worker.
An unsupported image, failed installation or pending MouseTweaks dependency
disables the native entry only. It cannot stop overlay initialization.

When admitted, native observation restricts the entry and menu input to the
verified foreground main/pause scene and its original identity. Temporary focus
loss clears presses, drag state and key capture, retains the page, and requires
physical inputs to be released before resuming. A different/invalid origin
closes the page. Without native admission the upstream general overlay remains
available; this fallback makes no claim to know a Bedrock scene's identity.
The user's game-input blocking preference remains effective in both paths.

## Provenance and boundary

The profile/guard/definition implementation originates from the MIT-licensed
[XuanDiNa/ModMenuBedrock](https://github.com/XuanDiNa/ModMenuBedrock/tree/10f80f266523d3196e1af1f212375abadfa7c9a1/editions/vanilla)
at commit `10f80f266523d3196e1af1f212375abadfa7c9a1`. The notice is retained in
[integration/ModMenuBedrock-LICENSE.txt](../integration/ModMenuBedrock-LICENSE.txt).
`NativeBridge.cpp` is a narrow adapter, without the previous UI renderer or old
pointer/Workspace hooks. It installs only the six definition/JSON/context/
Down/Up/controller hooks. The public ModMenu API is unchanged.

Each exact Minecraft image (1.26.45.01, 1.26.51.01, 1.26.52.03, 1.26.60.05)
has its own reviewed PE identities, complete file hashes, code hash, entry bytes,
RVA mappings and layout. The checked-in authorities are
[`NativeCompatibility.inl`](../src/compat/NativeCompatibility.inl) and
[`NativeGuards.inl`](../src/compat/NativeGuards.inl); their values are preserved
from the reviewed source. Address keys are lookup keys, never permission to use
an unreviewed address on a different game build. There is no pattern scanner,
arbitrary PE normalization or EXE modification.

The additional 26.60 launcher/PreLoader forms retain all eight original section
headers and payloads, entrypoint and existing imports. Only explicitly reviewed
complete-file hashes are added; a reconstructed launcher-before-PreLoader form
matches the player-reported complete-file hash. GUI/console variants are exact
hash entries, not a rule accepting arbitrary header changes. These data additions
are retained from the previously verified candidate, separately from the
original import. `Native60VariantsTests` checks each accepted form and negative
mutations using private copy-on-write mappings.

`NativeProfile` verifies disk identity, loaded headers (only the ASLR ImageBase
is compared in normalized form), live entry bytes and virtual-table ownership.
The shared Down/Up hook exceptions are the two explicit MouseTweaks file
hashes in `NativeBridge.cpp`; an unknown owner is refused. Companion readiness
waits occur on the optional worker, not the overlay thread.

The 26.52 HUD dependencies also have one explicit coexistence policy for the frozen
SodiumExtra 0.11.1-diag1 DLL (549,888 bytes, SHA-256
`C80F4F91FD0883D4424D72CEED5D984B90249FAB20AE2701302EC5A46D7CB044`).
Admission requires its ready HUD capability, a pinned module, the exact owned
scene-wrapper destination and its 22 live code bytes, the pop-wrapper destination
and all 69 live bytes (both return-address branches), and an unchanged complete
game function after each five-byte detour. The scene body includes the context
publication callsite. The retained binary's four-entry detour table, scene
forwarding helper and pop dispatcher were checked against source. Pop calls the
original with the same context exactly once before private HUD handling; menu
scenes cannot enter that HUD handling.

The complete companion HUD set was intersected with the ModMenu guards. Scene
and pop overlap; image/label do not. Text/fill/flush are unhooked dependencies and
remain subject to ordinary checks. The bridge covers both audited
overlaps and revalidates both live entry snapshots, relay destinations, wrappers
and full remaining function bodies before publishing a scene. Other builds,
owners, wrappers or body changes do not receive this exception. The default
profile verifier still rejects modified entries unless the bridge explicitly
supplies this audit. Successful native definition insertion is logged once per
main/pause namespace to distinguish native admission from actual button insertion.

## Resource packs and graphics

See [resource-pack integration](../integration/RESOURCE_PACKS.md) for the opt-in
`button.modmenu.open` event. The exact parser caller, controller and definition
ownership are checked before automatic insertion. On 26.60 the callback changed:
a bounded 216-byte record plus an exact vanilla payload hash supplies ownership.
Modified/layered definitions must provide their own explicit pack entry.

Public DXGI factory observation starts before slow profile work, and associates
a swapchain with its actual creation queue. No first-seen queue guess is used.
If creation observation is unavailable, initialization still attempts the
independent Present hooks. D3D11 rendering remains available; an unobserved D3D12
swapchain is forwarded without overlay drawing because its queue is unknown.
This fallback neither takes over another hook nor changes the game's renderer.
D3D12 entry points are resolved at runtime so a D3D11-only system can load the
DLL. F11 publishes an invalidation flag; resource cleanup stays on the serialized
render/resize path. A queue fence (D3D12) or event query (D3D11) must prove GPU
completion before release. A bounded failure retains resources, refuses resize
and disables overlay drawing/input rather than releasing in-flight buffers.
Original Present forwarding is outside the overlay resource mutex.

Input follows the foreground output window rather than permanently retaining
the first auxiliary swapchain's window. Later auxiliary Presents cannot steal
that binding. Per-window procedure predecessors preserve a later plugin's
subclass chain. Destroyed-window bindings are cleared before an HWND can be
reused. Release-gate maintenance runs with the menu closed, so returning focus
and releasing inputs does not require an already-open ImGui frame.
Diagnostics record the selected window class and at most eight attempts of the
configured opening key; they do not record text or unrelated keystrokes.

## Verification

`StartupTests` executes production initialization with inert platform services;
`MenuInputTests` executes production UI input methods. Native tests validate
exact profiles and mutation refusal against locally supplied game images.
`OverlayQueueTests.ps1` uses the actual overlay with a hidden independent graphics
host to check D3D11/D3D12 queues, failed probes, F11, resize and forwarding.
These checks do not start Minecraft and do not prove a reported player freeze or
NVIDIA access violation is repaired. Fullscreen, scene transitions and gameplay
acceptance remain manual tests. Private EXEs, IDA databases and dumps are not
included. Android does not compile this Windows native extension.

`NativeSharedEntryTests` and `NativeCoexistenceTests` use inert loader mappings
and copy-on-write pages to exercise the production profile/companion audit.
Neither game entrypoints nor companion initialization run. The companion
capability accessor is replaced privately by a six-byte inert fixture; no
original companion accessor is called. These tests do not verify SodiumExtra's
gameplay behavior. The full-set regression installs all four inert HUD detours.
All six native hooks are queued before one activation transaction, so the
definition loader cannot become active before its JSON reader. The native
worker starts before the hidden graphics probe. An already loaded exact
SodiumExtra companion may publish readiness later than two seconds. The worker
polls the owned capability with a 120-second total bound, then repeats complete
profile validation. It does not repeatedly hash the full game while waiting
or admit an unready/unknown owner. Queue failures and partial activation have
rollback regressions; pristine unowned dependencies remain checked at runtime.

Internal candidate reports and manual acceptance records are maintained outside
the public source tree. Offline regressions and gameplay acceptance are separate.
