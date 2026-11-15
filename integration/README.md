# Local compatibility interfaces

This candidate keeps upstream `include/modmenu/ModMenuApi.h` v1/v2 unchanged and adds compatibility for passive `modmenu.settings.json` v1–v5. The game DLL is **ModMenuBE.dll**, with **ModMenu_GetApi** as its only public export. It does not impersonate `ModMenuBedrock.dll`, expose `MMB_GetStatus`, or export Workspace functions.

- Use a passive descriptor for ordinary settings. The validated `plugin.id` is the installation folder ID and must match the manifest entry/version. Paths are JSON pointers, not arbitrary writes. The existing parser, UTF-8 limits and transaction safeguards are retained.
- Use `binding` schema v4 for Ctrl/Shift or v5 for Ctrl/Shift/Left Alt. Legacy `key` fields retain their existing declared allowed keys. `include/modmenu/ModMenuBinding.h` is the common keyboard-only contract.
- Use the existing upstream C API for settings backed by callbacks or an external-screen entry. Its existing immediate setter / onSave behavior is unchanged; passive Apply/Cancel semantics must not be assumed for C API providers.
- Resource packs can emit `button.modmenu.open` on the native main/pause screen. See RESOURCE_PACKS.md for the original event and ownership contract; references there to old diagnostics/DLL names are historical, so use the local testing instructions for this build.
- There is no embedded Workspace, map widget, action broker, login task or command/status protocol in this integration. Existing standalone provider UIs stay under their own ownership.

The JSON schemas, resource entry examples, slider/binding contracts and ordinary ExamplePlugin/AutoRestock/FastBlockBreak fixtures are retained from ModMenuBedrock (MIT) for compatibility testing. They are SDK examples only and are never installed as player plugins by the runtime package. See `docs/plugin-migration-plan.md` before updating providers.
