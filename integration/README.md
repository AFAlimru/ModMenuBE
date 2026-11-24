# Plugin settings compatibility

On Windows, an optional `modmenu.settings.json` sidecar provides draft-based
settings without a plugin callback or native game hook. Apply writes validated
changes; Back and Cancel discard the draft. File operations run on a bounded
worker queue, and stale or unsafe files are refused. Plugin DLLs must implement
their declared reload policy themselves.

| Schema | Additional controls |
| --- | --- |
| v1 | Boolean, integer, choice, declared single key |
| v2 | Integer and decimal sliders with exact steps |
| v3 | Bounded UTF-8 text |
| v4 | Ctrl/Shift keyboard combinations |
| v5 | Left Alt combinations and the public binding contract |

See the [examples](examples), [sliders](SLIDERS.md),
[binding contract](KEYBINDINGS.md), and the `modmenu.settings*.schema.json` files
in this directory. Exact plugin ID, entry and manifest version must match.
The public `ModMenu_GetApi` API and existing `modmenu.json` routes are unchanged;
a registered external Config screen retains priority over the passive fallback.

The Windows transaction backend is not compiled into Android. Android retains
its existing `modmenu.json`, C API and generic settings screens; these sidecars
do not advertise an unsupported Config route there. The shared settings and
binding header are portable. No plugin update is required for existing routes.

ModMenuBE keeps F8 as its default. Its own config stores an integer `hotkey` and
the separate `hotkeyModifiers` mask (Ctrl=1, Shift=2, Left Alt=4), so older readers
still accept the primary key. Existing local string-form bindings are migrated
when saved. Escape clears a binding; mouse buttons cannot be bound.

## 中文

Windows 可透過選用的 `modmenu.settings.json` 使用統一設定控件。修改先保存在
草稿，Apply 才寫入；返回與 Cancel 捨棄修改。背景工作佇列有數量上限，並拒絕
過期或不安全的檔案。插件仍須自行實作宣告的熱更新。

Android 保留原有設定入口，暫不提供此 Windows 檔案交易後端。現有 API 與
`modmenu.json` 不變。ModMenuBE 預設仍為 F8，整數主鍵與修飾鍵分開保存。

## Verification

Build with xmake, then build and run `SettingsCompatibilityTests`,
`ExternalSettingsTests`, `SettingsTransactionTests`, `TextEditorTests` and
`SettingsFlowTests` in Debug and Release. The tests use isolated temporary
files; they do not load Minecraft or change installed plugin settings.
