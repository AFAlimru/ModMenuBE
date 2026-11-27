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

The sidecar belongs in the plugin's own directory. ModMenuBE contains no built-in
field lists or specialized parsers for individual plugins. A missing or invalid
sidecar does not select a schema by plugin name; existing `modmenu.json`, C API
and generic config-file routes remain available. Authors can use the examples
here to ship their own exact-version description.

The Windows transaction backend is not compiled into Android. Android retains
its existing `modmenu.json`, C API and generic settings screens; these sidecars
do not advertise an unsupported Config route there. The shared settings and
binding header are portable. No plugin update is required for existing routes.

ModMenuBE keeps F8 as its default. Its own config stores an integer `hotkey` and
the separate `hotkeyModifiers` mask (Ctrl=1, Shift=2, Left Alt=4), so older readers
still accept the primary key. Existing local string-form bindings are migrated
when saved. Escape cancels capture and preserves the previous binding. Plugin
fields have a separate Unbind button when their description allows `NONE`;
Apply saves that draft and Cancel discards it. ModMenuBE's own menu hotkey cannot
be unbound, and an old unbound config recovers F8. Mouse buttons cannot be bound.

## 中文

Windows 可透過選用的 `modmenu.settings.json` 使用統一設定控件。修改先保存在
草稿，Apply 才寫入；返回與 Cancel 捨棄修改。背景工作佇列有數量上限，並拒絕
過期或不安全的檔案。插件仍須自行實作宣告的熱更新。

Android 保留原有設定入口，暫不提供此 Windows 檔案交易後端。現有 API 與
`modmenu.json` 不變。ModMenuBE 預設仍為 F8，整數主鍵與修飾鍵分開保存。

描述檔放在插件自己的資料夾內；本體不內建個別插件的欄位或專用解析器。
缺少或無效的 sidecar 不會按插件名稱套用預設欄位，原有 `modmenu.json`、C API
與通用設定檔入口仍可使用。作者可參考範例自行提供符合版本的描述檔。

捕捉按鍵時 Esc 取消並保留原本綁定。插件欄位只有在描述檔允許 `NONE` 時才可用
獨立的「解除綁定」按鈕；套用才儲存，取消捨棄草稿。ModMenuBE 自身熱鍵不可解除；
舊設定若為未綁定，讀取時恢復 F8。

## Verification

Build with xmake, then build and run `SettingsCompatibilityTests`,
`ExternalSettingsTests`, `SettingsTransactionTests`, `TextEditorTests` and
`SettingsFlowTests` and `SettingsUiTests` in Debug and Release. The tests use isolated temporary
files; they do not load Minecraft or change installed plugin settings.
