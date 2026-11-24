# Optional workspaces / 選用工作頁

Windows Workspace API v1 is optional and separate from ModMenuApi v1/v2.
Android does not currently support Workspace: its entry is hidden even when a
descriptor declares it, and `ModMenu_GetWorkspaceApi` returns null. The existing
Android Configure and public configuration API remain available.

## English

`modmenu.json` gains an optional `workspace` object. Missing/`false` means no workspace entry. A valid version-1 declaration exposes the workspace launcher for a loaded plugin, alongside Configure. Missing providers or targets are shown as unavailable; JSON alone cannot execute a function. Existing `config`, `open`, `commands`, C API v1/v2 and passive settings descriptors retain their own contracts.

```json
{
  "workspace": {
    "version": 1,
    "label": { "en_US": "Workspace", "zh_TW": "工作頁", "zh_CN": "工作页" },
    "entries": [
      { "id": "tasks", "type": "page", "target": "jobs", "label": "Tasks" },
      { "id": "editor", "type": "external", "target": "editor_ui", "label": "Open editor" }
    ]
  }
}
```

Entry IDs and targets are 1–64 ASCII letters/digits/underscore/dash; 1–16 entries are allowed. Captions are UTF-8 strings or translations with `en_US` fallback (160 bytes each). The author's own descriptor takes precedence over a community adapter. Invalid/empty/version-mismatched or duplicate declarations do not expose an entry. They do not disable ordinary settings. Targets are registered logical IDs belonging to this plugin; they are not paths, URLs, shell commands, DLL exports or pointers.

`ModMenu_GetWorkspaceApi(minVersion)` follows the same minimum-version rule as
`ModMenu_GetApi`: a minimum of 0 or 1 accepts the current v1 table; a minimum
above the supported version returns null. The first fields are always `version`
and `structSize`. Extensions append members, retain the existing prefix and
increment the advertised version when required. Check `version >= required` and
`structSize >= offsetof(lastNeededMember) + sizeof(lastNeededMember)` before
reading any function; never require exact size or copy `structSize` bytes into a
smaller local table. The stable v1 prefix ends at `returnToWorkspace`.

Service readiness depends on the installed overlay, not on optional exact-image
native hooks. Registration still checks the loaded owner, ordinary sibling
plugin folder and explicit descriptor. With native observation available, a
changed game scene/origin invalidates restoration. Without it, the upstream
general-overlay policy applies; the service does not claim to identify Bedrock
scenes. Focus loss itself never unregisters a provider or stops its jobs.

Resolve `ModMenu_GetWorkspaceApi` from the already-loaded `ModMenuBE.dll`, query version 1, check `version`, `structSize`, function ownership and `isReady`. See [the C header](../include/modmenu/ModMenuWorkspace.h) and [example](workspace-example/Example.cpp). Do not call from DllMain, rendering or input callbacks. Call registration on the plugin worker using an executable address in its own loaded module. The module, manifest name/version/entry and normal sibling plugin directory must match; successful registration pins the owner for the process lifetime. No hot unload or DLL impersonation is provided. This cooperative in-process API does not sandbox native DLLs.

Registration takes a copied declaration, e.g.:

```json
{
  "schemaVersion": 1,
  "plugin": { "id": "Example", "entry": "Example.dll", "versions": ["1.0.0"] },
  "externalTargets": ["editor_ui"],
  "pages": [{ "id": "jobs", "label": "Tasks", "controls": [
    { "id": "start", "type": "action", "label": "Start" }
  ] }]
}
```

The legacy workspace declaration/state/command v1 meanings are retained for the seven controls `text`, `boolean`, `action`, `status`, `progress`, `list`, `image`. `externalTargets` is a new optional list of up to 16 unique IDs. `pages` may be empty only when at least one external target is declared. A page target must match a declared page; an external target must match the registration. Form values are temporary command parameters; persistent settings stay in Configure.

The provider worker publishes complete immutable state with strictly increasing revision, polls explicit commands and publishes terminal request results. Registration, publication, opening, drawing, switching pages, focus loss and page closure never submit actions. No request retry or replay occurs. An action is held until its prior result finishes. A short/null Poll buffer reports required size including NUL and never removes a command. Input byte sizes exclude NUL; all buffers are copied synchronously with no retained pointer, callback, allocator or C++/Go object.

Snapshot and command examples:

```json
{"protocolVersion":1,"revision":1,"controls":{"start":{"enabled":true}},"results":[]}
```

```json
{"protocolVersion":1,"session":1,"requestId":7,"revision":1,"page":"jobs","action":"start","values":{}}
```

Lists add a stable `rowId`, not an array offset. Result records contain requestId, `succeeded`/`failed` and optional localized message. The provider revalidates arguments and current business state. Delivery is not task completion.

External entry activation first suspends ModMenu input and waits for all physically held keys/buttons to release, then Poll returns:

```json
{"protocolVersion":1,"session":1,"requestId":9223372036854775809,"kind":"openExternal","target":"editor_ui"}
```

Use real uint64 parsing (not JavaScript double). The provider worker opens its own UI and calls `acknowledgeExternal(session, requestId, 1)` only after it is created, or `0` on failure. Preparation/queued/delivered handoff times out after 15 seconds; expired commands cannot be delivered later. Duplicate/late acknowledgements are refused. No callback or network/file work is invoked by the renderer. One global UI handoff is active at a time.

The plugin's explicit Return button closes/hides its UI and calls `returnToWorkspace(session, requestId)`. ModMenu restores the saved page only when the original admitted scene/provider session is still valid, and waits for inputs to release. Closing the external window or losing focus does not imply Return or Stop. Your configured shortcut/normal Mods entry can explicitly reopen the menu and abandon the old handoff. A changed scene/session cancels restoration; plugin background jobs remain provider-owned.

Limits: 16 providers, 8 pages/64 controls/one image per provider; declaration 64 KiB, state 256 KiB, command including NUL 32 KiB, JSON depth 16, 16 pending actions; each list 128 rows/256 rows total, text up to 4096 UTF-8 bytes. Image is one 1–64 by 1–48 RGBA8 uint32 `0xRRGGBBAA` raster, rendered at most 4 updates/sec. The bounded pixels are drawn with the current ImGui draw list: no external texture/GPU pointer or file/URL decoding, no retained GPU resource. Invalid data retains the last valid snapshot.

API codes: 0 success, 1 short output, -1 invalid arguments, -2 menu not Ready, -3 identity/session/data refused. Never send credentials, cookies, access/refresh tokens or passwords in descriptors, state, commands or ordinary logs/config. Plugins own secure authentication.

Existing BedrockTool or other providers that pin `ModMenuBedrock.dll`, its four old exports or a specific binary need an explicit adapter update and new admission review in their own project. The new API is optional and does not make old binaries compatible. This package changes ModMenu only and contains an optional separate demonstration DLL.

## 繁體中文

在 `modmenu.json` 明確加入 `workspace` 才顯示工作頁入口；缺少或 `false` 不顯示。內嵌分頁與外部 UI 按鈕可同時宣告，一般 Configure 保持獨立。合法入口沿用 ModMenuBE 的 OreTheme、語言、返回及縮放；未就緒目標停用，不能只憑 JSON 呼叫函式。`page` 對應插件宣告的工作頁，`external` 對應同插件註冊的邏輯目標，不接受路徑、URL、匯出函式名或 shell 命令。

插件 worker 查詢 `ModMenu_GetWorkspaceApi(1)`，驗證版本、表大小、歸屬及 Ready，再用自己的模組程式碼地址註冊。DLL／相鄰目錄／manifest 身分須相符，成功後固定模組至程序退出。普通設定與舊 C API 不變；舊 Workspace 的 DLL 名稱、匯出及精確雜湊不會被偽造，舊提供者必須另外適配。

UI 只讀不可變快照、維護本地表單及排入明確使用者操作。登入、網路、檔案及任務由插件 worker 執行。開頁／重繪／刷新／關頁／失焦不觸發任務、不自動重試或 Stop。短 Poll 緩衝不移除命令，結果必須明確回報；送達不代表完成。

外部 UI 會先撤銷 ModMenu 輸入並等待全部按鍵釋放，再送出 `openExternal`。插件建立介面後確認成功／失敗；15 秒未確認即逾時，過期命令不能延遲執行。插件自己的「返回工作頁」按鈕才呼叫返回接口；一般關窗／失焦不自動返回。原場景或 session 改變不恢復舊頁面，背景任務仍繼續。也可明確用 原有快捷鍵／Mods 重新打開選單，放棄舊交接。

首版提供文字、開關、按鈕、狀態、進度、清單及有界像素預覽。最多 16 提供者、每個 8 頁／64 控件／1 圖、16 入口、16 等候動作；描述 64 KiB、快照 256 KiB、命令含 NUL 32 KiB。圖像最多 64×48、每秒最多更新 4 次；不讀圖片路徑或 URL、不接收外部 GPU 指標。請用獨立示範先測通，再於各插件專案適配及驗收。

Android 暫不提供工作頁：即使描述檔宣告也隱藏入口，獨立 API 查詢回傳 null；
原有 Configure 與設定 API 保留。API 採最低版本查詢與追加欄位規則，使用者必須
檢查版本及欲使用欄位的結尾位移，不應要求表大小完全相等。服務就緒不依賴選用的
原生映像掛鉤；原生場景可用時保留其保護，否則沿用上游通用 overlay 的範圍。

## Verification

Build and run `WorkspaceEntryTests`, `WorkspaceViewTests`,
`WorkspaceProtocolTests`, `WorkspaceHandoffTests`, `WorkspaceApiTests`,
`WorkspaceUiTests` and `MenuInputTests` in Debug and Release. The API test
compiles the public header as C and C++, checks minimum versions/prefix layout,
and uses an inert owner DLL for real identity/descriptor checks. UI tests execute
the production renderer and frame lifecycle offscreen; opening/redrawing cannot
submit actions. Android's `WorkspacePlatformContract` is a compile-only target
using the actual NDK. These checks do not start Minecraft or establish gameplay
acceptance.

The bounded protocol/broker and types are adapted from MIT-licensed
XuanDiNa/ModMenuBedrock commit `10f80f266523d3196e1af1f212375abadfa7c9a1`
(`editions/vanilla`), with local opt-in routing, external handoff and OreTheme UI
adapters. The previous Workspace export names/renderer/input hooks are not
imported. See [the retained notice](ModMenuBedrock-LICENSE.txt).
