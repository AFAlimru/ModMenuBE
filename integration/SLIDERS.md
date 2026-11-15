# Sliders / 滑動條

## English

ModMenuBedrock **0.5.0-test1** introduces passive settings description **v2**.
Only opt-in `type: slider` fields use sliders. Existing v1 files and built-in
adapters keep their current controls and behavior; no companion update is forced.
The menu writes configuration only. A companion must really understand the
declared field and reload policy. A successful save is not runtime acknowledgement.

Use `schemaVersion: 2` in `modmenu.settings.json`, with the same plugin identity,
exact manifest versions, config, translations and filesystem rules as v1.
Use the separate `modmenu.settings.v2.schema.json` for editor assistance.
Do not change the v1 schema or add slider members under schemaVersion 1.
Old ModMenu will refuse Configure for a v2 description, not reinterpret it.

```json
{
  "path": "/scale", "type": "slider", "numberType": "decimal",
  "precision": 2, "min": 0.5, "max": 2, "step": 0.05, "default": 1,
  "unit": "x", "label": {"en_US": "Scale", "zh_TW": "比例", "zh_CN": "比例"}
}
```

- `numberType: integer`: signed int32 numeric parameters/config; omit precision.
- `numberType: decimal`: required precision 1..6; actual values remain in
  -2147483648..2147483647. Values must be exactly representable at that precision.
- Required min/max/step/default. min < max, step > 0, step <= range and int32
  maximum; range must be a whole number of steps. Default and current value
  must equal min + k*step within bounds. No silent rounding of an invalid config.
- Decimal JSON exponent notation is accepted when exact at the precision.
  Direct input uses ASCII digits, optional leading minus and a decimal dot;
  no exponent, thousands separators or localized comma. Trailing zeroes are OK.
- Slider saves are real JSON numbers, never strings. Arithmetic uses scaled
  integers, with canonical decimal output and no accumulated float drift.
- The editor schema cannot enforce cross-field step/grid or exact raw decimal
  precision. The runtime parser is authoritative. All v1 security limits remain.

Drag the rail or click it to set a value; release outside still ends the drag.
Click the separate number box to type. Left/Right adjust one step, Shift ten,
Home/End choose endpoints, Enter confirms text. Escape during dragging restores
the pre-drag value; during typing it cancels only that input. Outside an active
control Escape leaves Configure. Focus/scene loss discards the page; resize
cancels an active drag. Reset affects the draft. Apply saves; Cancel does not.

`v2-examples/ExamplePlugin` is an author template with signed/decimal controls,
not a loadable plugin and intentionally has no dummy DLL. Adapt it to your own
real plugin/parser; merely copying it does not create a catalog entry.

`v2-examples/AutoRestock/modmenu.settings.json` is an **optional** integer-slider
test for the installed manifest version `0.3.1-test4` only. It exposes the same
enabled/threshold fields as the old example; it does not change AutoRestock code
or its matching rules. No file is installed automatically. Back up the existing
descriptor (or record that it was absent), then manually copy this one into the
real AutoRestock folder if you choose to test it. Preserve its DLL, manifest and
config. To undo, restore the old descriptor, or remove only the new descriptor
if none existed. A plugin with a different manifest version is intentionally
refused; never fake its version to make it match.

## 繁體中文

**0.5.0-test1** 新增被動設定描述 **v2**；只有明確宣告 `type: slider` 的欄位
使用滑動條。既有 v1、內建適配器及其他插件不會自動換控件，也不強制更新。
ModMenu 只寫入設定；插件必須真的支援欄位及熱載入，儲存成功不代表收到生效回覆。

描述檔名稱仍是 `modmenu.settings.json`，根節點使用 `schemaVersion: 2`。
插件身份、精確 manifest 版本、設定檔、翻譯和安全規則沿用 v1；編輯器搭配獨立的
`modmenu.settings.v2.schema.json`。不可在 v1 偷加滑動條。舊 ModMenu 會拒絕
該 v2 設定頁，不會猜測格式或影響插件 DLL 載入。

- `numberType: integer`：正負 int32 整數，不填 precision。
- `numberType: decimal`：必填 precision 1–6，數值範圍仍為
  -2147483648 至 2147483647，必須能以指定精度精確表示。
- min、max、step、default 必填；min 小於 max，step 為正且不超過範圍及 int32
  上限。整個範圍、預設值、目前值都必須符合從 min 起算的步進；不偷偷修正設定。
- 小數 JSON 可用能精確表示的科學記號；直接輸入只接受數字、前置負號及小數點，
  不接受科學記號、千分位或小數逗號。多餘的尾端零可以接受。
- 使用固定精度整數運算，寫回真正的 JSON 數字，不是字串，不累積小數誤差。
  步進／精度交叉限制以執行期解析器為準，JSON Schema 僅供編輯器輔助。

拖曳或點擊滑軌可調整，滑出軌道放開也會結束拖動。點旁邊數字可直接輸入。
左右鍵調整一步，Shift 十步，Home／End 到兩端，Enter 確認輸入。
拖曳時 Escape 還原本次拖動；輸入時只取消該次輸入；沒有操作控件時才離開設定頁。
失去焦點／切換場景沿用原本丟棄設定頁的規則；調整視窗大小取消當前拖動。
重設只改草稿，Apply 儲存，Cancel 不儲存。

`v2-examples/ExamplePlugin` 是作者範本，不是可載入插件，刻意不附假 DLL。
`v2-examples/AutoRestock/modmenu.settings.json` 是**選用測試描述檔**，僅對應
目前 manifest 為 `0.3.1-test4` 的 AutoRestock。它不修改補貨程式及匹配規則。
若要測試，先備份既有描述檔（原本沒有也請記下），再手動複製此檔；保留 DLL、
manifest 及 config。還原時放回舊描述檔，原本沒有才移除新加的描述檔。
版本不同時應停止，不要偽造 manifest 版本。本套件不會自動安裝任何描述檔。
