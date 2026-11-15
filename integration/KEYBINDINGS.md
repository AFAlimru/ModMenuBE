# Keyboard bindings / 鍵盤綁定

## English

ModMenuBedrock **0.6.0-test5-leftalt** adds opt-in settings v5 `binding` fields.
The existing v1/v2/v3/v4 `key` contracts remain unchanged. A matching plugin runtime
must adopt the parser and trigger semantics before shipping a v5 description.
Frozen v4 supports only Ctrl/Shift chords; v5 opts into SDK contract 2 LeftAlt bit 4.
Legacy `CTRL+LALT` remains a Ctrl chord with Left Alt as its primary key.
Changing a sidecar alone cannot add combinations to an older DLL.

```json
{"path":"/toggleKey","type":"binding","default":"NONE","label":"Toggle key"}
```

Use `schemaVersion: 5`, the normal exact plugin ID/entry/version declaration,
and [the v5 schema](modmenu.settings.v5.schema.json). Copy
[ModMenuBinding.h](sdk/ModMenuBinding.h) into your own source tree; it has no
Windows headers or dependency on a running ModMenu DLL. It is MIT licensed under
the [project license](../LICENSE).

- Canonical values: `NONE`, `K`, `CTRL+K`, `SHIFT+K`, `CTRL+SHIFT+K`, `LALT+K`, `CTRL+LALT+K`,
  `SHIFT+LALT+K`, `CTRL+SHIFT+LALT+K`.
  At most 48 bytes; Ctrl then Shift then Left Alt, then one primary key. Only Left Alt
  is an Alt chord modifier; Right Alt/AltGr cannot substitute for it. Windows
  keys and Right Alt remain observable standalone primary keys.
- Letters, digits, F1–F24, editing/navigation, numpad, punctuation, locks,
  standalone modifiers and observable media/browser keys are supported. Use
  `OEM_PLUS` for the plus key and assigned `VK_XX` fallback names returned by
  `KeyToken`. Escape, mouse buttons, unassigned and sentinel codes are refused.
- Click the control, hold Ctrl/Shift/Left Alt, then press the primary key. A modifier
  alone is captured on release. Esc selects Unbound without saving; Backspace
  is bindable. Mouse clicks never become a binding. Apply saves; Cancel discards.
- Matching uses the **exact Ctrl/Shift/Left Alt mask**. `CTRL+K` does not match bare K
  or Ctrl+Shift+K. The required modifiers must precede a fresh primary press.
  Late modifiers, repeats and preheld keys after focus/config changes do not
  manufacture an action. A hold ends when any required key stops matching.
- Keep plugin eligibility checks. Call `Tracker::Update` even on ineligible
  samples, passing `false`, so focus/UI/config changes can clear input state.
  Reset after configuration revisions that should re-prime the shortcut.
  ModMenu does not dispatch actions or arbitrate conflicts between plugins.
- Windows-reserved shortcuts and firmware-only Fn cannot be guaranteed. Keys
  that are consumed by Windows may never reach the game. No OS interception is
  added by this contract.

```cpp
auto binding = modmenu::binding::ParseInput(savedToken); // refuse invalid config
if (!binding) return;
auto keys = modmenu::binding::Sample(*binding, readPhysicalKey);
auto state = tracker.Update(*binding, keys, existingEligibility);
if (state.pressed) { /* plugin-owned toggle/action */ }
// state.held is for an existing hold feature; it does not change that feature.
```

`Parse` reads strict tokens; description defaults must equal `Token(Parse(value))`.
`ParseInput` additionally accepts documented legacy spelling aliases. Numeric VK
JSON can be read without rewriting it until edited. Only plugins that previously
read base-0 numeric strings should declare `legacyNumericStrings: true`: e.g.
`"56"` retains VK 56; a newly captured main-row 8 saves as `"VK_38"` to avoid
ambiguity. New plugins should leave this compatibility option absent.

The menu's own `openKey` keeps numeric single keys and saves chords as strings.
Independent plugins retain their existing game-version support. This feature
adds no game versions, native offsets, hooks or synthetic output combinations.

## 繁體中文

**0.6.0-test5-leftalt** 新增選用的 v5 `binding`。舊 v1／v2／v3／v4 `key` 規格不變。
v4 維持只有 Ctrl／Shift 組合；v5 才啟用左 Alt，右 Alt／AltGr 不代替左 Alt。
插件必須同步更新解析與觸發邏輯，才能宣告新版描述檔；只更換描述檔無法讓舊 DLL 支援組合鍵。

儲存格式為 `NONE`、`K`、`CTRL+K`、`SHIFT+K`、`CTRL+SHIFT+K` 及左 Alt 組合（如 `LALT+K`、`CTRL+SHIFT+LALT+K`），最多 48 bytes。
一般鍵盤鍵、F1–F24、數字小鍵盤、標點、編輯／導覽鍵及可觀測的媒體鍵均可使用。
Esc 與滑鼠按鍵不可綁定；Esc 設為無綁定，Backspace 可作為正常快捷鍵。

點擊綁定欄位後，按住 Ctrl／Shift／左 Alt，再按主鍵；只綁 Ctrl／Shift／左 Alt 時，放開後確認。
套用才儲存，取消捨棄草稿。修飾鍵必須完整且精確符合；先按住 K 再補 Ctrl 不觸發，
持續按住也不重複切換。失焦及設定變更後需重新放開再按。

使用上方共用標頭與 v5 範例接入，保留插件原有的前景、世界及介面限制。
ModMenu 不代替插件執行功能，也不統一攔截其他插件的快捷鍵。Windows 保留組合與韌體 Fn
無法保證能收到。此更新沒有新增遊戲版本支援或改動插件的遊戲功能。
