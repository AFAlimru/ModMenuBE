# Resource-pack entry API v1 / 材質包入口介面 v1

[English](#english) · [繁體中文](#繁體中文)

## English

**Introduced in 0.6.0-test7-resourcepack.** Optional Bedrock JSON UI integration:
the pack draws and places a button; ModMenu handles its explicit click. No
LeviLamina, server protocol, scripts, DLL callbacks or native offsets are needed
in a pack. The normal ModMenu shortcut remains available in allowed scenes;
automatic vanilla entry insertion keeps its existing layout and layer checks.
Existing settings/workspace APIs are unchanged.

### Button event

Send **`button.modmenu.open`** from a normal JSON UI button. The old
`button.mmb_open_mods` event remains an alias. The v1 action opens the existing
Mods page; it has no arguments and cannot submit a plugin command, change a
setting, start a task or force a native screen open.

The engine renders the pack's button, including its normal/hover/pressed states,
textures, text, size, anchors, offsets, visibility and focus. ModMenu receives the
ordinary mapped event after a real user action. It neither executes pack code nor
reads a pack installation folder.

Use the template in
[modmenu_entry_v1.json](resource-pack-entry/example-pack/ui/modmenu_entry_v1.json):

```json
{
  "my_mods@modmenu_entry_v1.button": {
    "size": [110, 24],
    "anchor_from": "top_left",
    "anchor_to": "top_left",
    "offset": [8, 8],
    "$button_text": "Mods",
    "$focus_id": "my_pack_mods"
  }
}
```

Copy the template file into your pack and **add** its path to your own
`ui/_ui_defs.json` list. Do not replace your existing list with the example list.
Instantiate the button once in the active menu's content controls. For a fully
custom button, retain these mappings on your own `type: button` control:

```json
"button_mappings": [
  { "from_button_id": "button.menu_select", "to_button_id": "button.modmenu.open", "mapping_type": "pressed" },
  { "from_button_id": "button.menu_ok", "to_button_id": "button.modmenu.open", "mapping_type": "focused" }
]
```

Do not use a `global` mapping: opening Mods is a button action, not a replacement
for the menu's normal Confirm key. Do not change the event name when reskinning
the supplied template. The focus mapping is authored and checked offline;
controller/keyboard activation still needs an in-game test on the target layout.

### Ownership and pack stacking

For each menu you manage, add this unused, hidden control definition at the root
of **that menu's namespace** (`start` or `pause`):

```json
"mmb_resource_pack_entry_v1": { "type": "panel", "visible": false }
```

This is an ownership marker, not the visible button. It suppresses ModMenu's
automatic entry insertion for that definition; it never grants permission to
open from a different scene. Put your visible button in the active control tree.
ModMenu preserves the supplied JSON. The reserved `mmb_` collision policy also
keeps unsupported marker versions from silently re-enabling insertion.

Multiple resource layers continue to use the engine's own merge path; ModMenu
does not rewrite them. A pack-authored button sends the event directly and does
not depend on ModMenu's automatic vanilla-layout insertion. This provides an
explicit integration route for modified menus; it does not make every unmodified
custom pack compatible. The pack controlling the final layout must retain the
button. Combine integration with that pack rather than stacking conflicting
complete menu replacements, and keep one visible entry per menu.

### Scope and availability

- Exact Windows x64 games: **1.26.45.01 / 1.26.51.01 / 1.26.52.03 / 1.26.60.05**.
  26.60 is added by the local test8 candidate; manual acceptance is pending.
- Current allowed scenes: the verified native **main and pause menus**. The
  foreground, top-scene, controller identity and viewport guards still apply.
- HUD, inventory, settings, OreUI/HTML screens and replaced/unrecognized native
  controllers are outside v1. Copying the event there does not grant access.
- There is **no `#modmenu_available` binding** in v1. The example is an optional
  companion pack for users who have enabled a compatible ModMenu DLL. Without
  it, the button may still be drawn but cannot open Mods. A pack manifest cannot
  declare a Bedrock dependency on a native DLL; communicate that requirement.
- Language is handled by the pack's `.lang` files. The native Mods page continues
  to use the game's selected language independently.
- Restart Minecraft after changing the native DLL or UI pack; F10 is not a UI
  resource reload. Preserve existing ModMenu configuration.

The machine-readable [entry.v1.json](resource-pack-entry/entry.v1.json) describes
this public contract. It is documentation for tooling; ModMenu does not discover
or execute that JSON. It is separate from `modmenu.settings.json`.

### Example and acceptance

The [example pack](resource-pack-entry/example-pack/manifest.json) appends a
96×24 button at the top left of both vanilla content panels. It supplies only
original integration fragments, a reusable button template and three language
files; it does not redistribute vanilla UI definitions or textures. For a
different layout, adapt the insertion targets and position in your own pack.
Enable it under **Global Resources**, not only inside a world.

Test main-menu click → Mods → Done, pause-menu click → Mods → Escape (still
paused), the configured shortcut, GUI scale changes and your final pack stack.
Also confirm settings/inventory/HUD do not gain an entry, no duplicate appears,
and disabling the example pack restores normal vanilla integration after a
restart. Engine merging, visuals and actual click delivery require manual game
testing; offline checks alone are not acceptance.

## 繁體中文

**自 0.6.0-test7-resourcepack 提供。** 材質包使用基岩版 JSON UI 繪製及安排按鈕，
使用者點擊後，由 ModMenu 開啟既有 Mods 頁面。不需要 LeviLamina、服務端協定、
腳本或原生位址；其他插件的設定／Workspace 介面保持相容。

### 接入方法

1. 複製上方連結的 `ui/modmenu_entry_v1.json`，把路徑**追加**到自己材質包的
   `ui/_ui_defs.json`，保留原有清單。
2. 在主選單或暫停選單的實際控制項中加入 `自訂名稱@modmenu_entry_v1.button`。
   可自行修改文字、尺寸、位置、貼圖及各種按鈕狀態。上方範例可直接修改。
3. 在該選單的 `start`／`pause` 命名空間頂層加入
   `"mmb_resource_pack_entry_v1": { "type": "panel", "visible": false }`。
   這只是接管標記；可見按鈕仍需放進實際控制項，避免 ModMenu 再自動插入一個入口。

自行製作按鈕時，讓一般按鈕映射送出 **`button.modmenu.open`**；舊名稱
`button.mmb_open_mods` 仍可用。`menu_select` 使用 `pressed`，`menu_ok` 使用
`focused`，不要改成整個畫面都生效的 `global` 映射。介面只提供「開啟 Mods」動作，
沒有參數，也不會執行插件工作或修改設定。

### 多層材質包與相容範圍

多層 UI 仍由遊戲自行合併；材質包按鈕直接使用公用事件，不依賴 ModMenu 的原版
排版自動插入流程。這能讓修改過選單的材質包**主動接入**，並不會自動替所有未適配
材質包加上按鈕。最終控制選單的材質包仍須保留入口；建議直接整合進該材質包，
每個選單只保留一個可見入口，避免多個完整選單包互相覆寫。

精確遊戲版本：**1.26.45.01／1.26.51.01／1.26.52.03／1.26.60.05**。26.60 自 test8
候選版加入，人工驗收待完成。入口只在經驗證的
原生主選單與暫停選單生效，仍檢查前景、頂層場景及原生控制器。HUD、背包、設定、
OreUI／HTML 或替換了原生控制器的畫面不在 v1 範圍內。

v1 **沒有 `#modmenu_available` 可用性綁定**。範例包需要已啟用相容的 ModMenu DLL；
沒有 DLL 時按鈕仍可能顯示，但不會開啟 Mods。基岩版材質包 manifest 無法宣告原生
DLL 依賴，作者應在說明中列明。按鈕文字由材質包的 `.lang` 管理，ModMenu 內頁則
繼續跟隨遊戲語言。更新 DLL 或 UI 材質包後請重啟遊戲，保留原有設定。

### 範例包與測試

範例在主選單與暫停選單左上角追加 96×24 按鈕，提供英文、繁體與簡體文字。
只包含自行撰寫的 UI 片段，沒有複製原版完整選單或貼圖。請放在**全域資源**啟用；
自訂排版的作者需要調整插入目標與位置。

請測試兩個選單的點擊、Done／Escape 返回、原有快捷鍵、GUI 比例與實際材質包
組合；從暫停選單返回後仍應暫停。確認沒有重複入口，設定／背包／HUD 不會新增入口，
停用範例並重啟後原版入口仍正常。離線驗證不代表遊戲內合併、顯示及點擊已驗收。

公用規格索引為上方 `entry.v1.json`，它供作者與工具閱讀，並非執行檔或設定描述檔。
