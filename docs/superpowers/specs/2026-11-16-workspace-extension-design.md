# 通用內嵌工作頁擴充設計草案

日期：2026-11-16。狀態：使用者已確認，開始獨立本地實作，未提交上游。已納入使用者補充的 `modmenu.json` 明確宣告與多種 UI 入口需求。

## 需求與現況

使用者希望合併後的 ModMenuBE 仍可承接我們需要工作頁的插件。目前 local3 只有一般設定及 upstream external-screen 入口。`setExternalScreen` 會關閉 ModMenuBE 並呼叫插件的開啟函式，不能提供選單內的動態頁面、命令結果或地圖。

普通開關、滑動條、文字及快捷鍵繼續走現有 Configure。工作頁只供需要操作流程、任務狀態及動態資料的插件選用，不要求所有插件更新。使用者指定：`modmenu.json` 未包含工作頁宣告時不顯示入口；有宣告時顯示，打開後保持 ModMenuBE UI，同時可提供按鈕跳轉至其他 UI。

## 路線比較

| 路線 | 適用與限制 |
| --- | --- |
| 各插件使用 external-screen | 可保留插件自己的介面，但每個插件要自行維護繪製、輸入與生命週期，不能提供一致的內嵌工作頁。 |
| 通用內嵌工作頁（建議） | ModMenuBE 統一呈現及輸入；插件自行執行工作。可復用舊工作頁資料與校驗模型，需要新版本化入口及提供者適配。 |
| 整套移植舊 Workspace/UI/ABI | 舊協定表面相近，但引入另一套繪製／輸入耦合；舊插件仍固定 DLL 名稱與雜湊，不能保證直接相容。 |

建議第二條：復用已存在的宣告、快照、命令仲介與驗證邏輯，在 ModMenuBE 的 ImGui／OreTheme 中重建呈現層。舊選單 renderer、pointer bridge 和 game hooks 不移入。

## 宣告與入口顯示

在插件的 `modmenu.json` 新增選用的 `workspace` 區塊，與現有 `config`、`open`、`commands` 分開。以下為擬定格式，尚未形成已實作接口：

```json
{
  "workspace": {
    "version": 1,
    "label": { "en_US": "Workspace", "zh_TW": "工作頁", "zh_CN": "工作页" },
    "entries": [
      {
        "id": "tasks",
        "type": "page",
        "target": "jobs",
        "label": { "en_US": "Tasks", "zh_TW": "任務", "zh_CN": "任务" }
      },
      {
        "id": "editor",
        "type": "external",
        "target": "editor_ui",
        "label": { "en_US": "Open editor", "zh_TW": "開啟編輯器", "zh_CN": "打开编辑器" }
      }
    ]
  }
}
```

- `workspace` 缺少或為 `false`：插件詳情不顯示工作頁入口。僅呼叫 runtime 註冊不會替未宣告的插件加按鈕。
- 合法 v1 宣告含至少一個入口：已載入插件顯示工作頁按鈕；未載入插件不作為可操作提供者列出。
- 宣告存在但提供者尚未就緒、目標未註冊或版本不相容：對應入口停用並顯示原因，不把宣告當成可執行能力。
- 空宣告、錯誤類型、未知版本、重複 ID 或超限資料不生效並給出有界診斷；其他既有設定能力不受該擴充錯誤影響。
- 作者的 `modmenu.json` 優先於社群 adapter，沿用目前選檔規則。社群 adapter 可明確提供同一區塊；不掃描其他任意 JSON 猜測工作頁。
- `entries` 為 1–16 個穩定 ID，`target` 只對應同一插件已宣告／註冊的邏輯目標。不接收 DLL 地址、匯出名稱、檔案路徑、URL 或任意 shell 命令作為 UI 目標。
- 原有 `open` 與 `setExternalScreen` 保持現有語意，不自動轉成工作頁宣告。普通 Configure 不會因新增 Workspace 而被遮蔽。

## 使用者介面

- 入口是否出現由合法宣告決定，入口是否可操作由對應提供者／目標就緒狀態決定。插件詳情中的 Configure 與「工作頁／Workspace」並存。
- 點工作頁先進入 ModMenuBE 風格的入口頁，以按鈕／卡片排列宣告的各個入口。首版不允許任意自訂 renderer；統一使用現有 OreTheme 的字體、間距、按鈕、捲動、語言及返回方式。
- `page` 入口在同一選單內打開提供者宣告的原生工作分頁，保留頁籤、草稿及捲動；返回依序回入口頁、插件詳情。
- `external` 入口明確標示將離開目前工作頁並打開插件自己的 UI。可以只宣告多個 external 入口，作為插件各種操作介面的啟動頁，不強迫插件重寫已有 UI。
- 首版沿用七類通用控件：`text`、`boolean`、`action`、`status`、`progress`、`list`、`image`。不寫死任何插件、伺服器或帳戶功能。
- 動作顯示忙碌、停用原因、錯誤及可選的危險確認。列表具穩定 row ID、選取狀態、空清單及有界捲動。
- image 僅接受有界 RGBA8 像素；不讀 URL／路徑、不接收外部 GPU 指標。首版維持一張最大 64×48、每秒最多 4 次呈現更新。
- 工作表單屬暫態命令參數；持久設定維持 Configure，避免同一設定有兩份互相覆蓋的草稿。

## 外部 UI 交接

外部跳轉只由明確按鈕／鍵盤啟用觸發。`external` 的目標 ID 必須由該插件的工作執行緒註冊，不能藉 JSON 呼叫其他插件或偽造函式地址；ModMenuBE 使用有界命令／交接回覆通道，不把登入或建立視窗等工作放進繪製回呼。

交接須有明確 requestId、成功／失敗和逾時回覆，並防止同一點擊重送。成功交接前保存原工作頁位置並撤銷 ModMenuBE 輸入所有權，清除按下配對且等待釋放，避免同一個點擊穿透到新 UI。失敗或逾時不自動重試，顯示可見結果。發送請求不等於外部 UI 已建立。

外部介面完成後可透過明確返回請求回到原工作頁位置；原場景及 provider session 必須仍有效。不因關窗、失焦或背景任務完成而自動打開選單。插件控制自己的 UI 外觀；ModMenuBE 只統一其入口、內嵌頁面及交接行為。

先支援同插件的內嵌頁與外部 UI；瀏覽器登入等系統操作繼續由插件 worker 在原有授權流程處理，不由通用工作頁直接執行網址或命令。

## 版本化接口與工作執行緒

建議新增獨立 C ABI 查詢入口 `ModMenu_GetWorkspaceApi(minVersion)`，回傳含 `version`、`structSize` 及 Register／Publish／Poll／Unregister 的工作頁 v1 函式表。名稱與 ABI 在本設計核准後才凍結；原有 `ModMenu_GetApi` v1／v2 欄位及行為保持不變。

新插件明確查詢能力；缺少接口時保留核心功能並顯示未接入狀態，不載入另一份選單。函式表亦須提供具版本／structSize 的外部目標註冊、交接確認及明確返回能力，具體 ABI 在實作前一併凍結。可見提供者必須對應實際載入插件、manifest 名稱／版本及 DLL 歸屬；註冊／模組存活檢查由平台層負責，純資料協定與仲介避免依賴 Win32。

流程：插件 worker 註冊宣告及發布不可變完整快照 → UI 讀取快照和本地草稿 → 使用者明確啟用動作 → 有界命令佇列 → 插件 worker Poll、重新驗證並執行 → 發布成功／失敗結果。

所有緩衝於呼叫內複製；不跨 ABI 傳 Go 指標、C++ 物件、allocator 所有權或工作回呼。繪製執行緒不呼叫插件登入、網路、檔案、Go 或其他長任務。只有使用者啟用動作或宣告的列表選取才產生命令；開頁、讀取、重繪、更新狀態、切頁及失焦不產生命令。

保留 session、嚴格遞增 revision、requestId、原頁表單與穩定 row ID。短 Poll 緩衝不取走命令；等待同一動作結果期間不重送，沒有自動重試或重播。送達不代表完成，插件必須重新驗證參數及當前業務狀態。

## 界限與生命週期

首版沿用舊協定的有界限制：16 個提供者；每提供者 8 頁、64 控件、1 張圖；宣告 64 KiB、快照 256 KiB、命令含 NUL 最多 32 KiB；16 筆等候／執行請求；每清單 128 列、總共 256 列；文字最多 4096 UTF-8 bytes，JSON 深度 16。

無效 UTF-8、未知屬性、重複 JSON 成員、超限／錯誤類型或過期版本整筆拒絕，保留上一份有效快照。控件錯誤不允許其他插件覆寫。

關頁不取消任務、不自動 Stop、不取消註冊。暫時失焦保留有效頁面、捲動及已完成的本地表單值，同時取消按下配對、拖曳、綁鍵、未完成輸入及危險確認。恢復必須通過原有前景／主或暫停場景／origin 檢查及按鍵全放開屏障。換場景、提供者註銷或 session 改變時丟棄舊頁面；已交付任務仍由提供者處理。

不把憑證、權杖、Cookie 或密碼放入設定、宣告、快照、命令及一般日誌。公開工作頁不持久化狀態 payload；安全登入及憑證存放仍由插件負責。

## 相容性與交付界線

- 原有 upstream API v1／v2、被動設定 v1–v5、材質包事件與所有權標記保持相容。
- 不冒充 `ModMenuBedrock.dll`，不偽造舊 Ready 狀態／雜湊，不移除提供者的 dependency guards。
- 舊工作頁 JSON／命令資料盡量保持 v1 語意，減少提供者改動；保留 wire format 不等於舊 DLL 可直接載入。
- 固定舊 DLL 名稱、四個舊匯出或精確雜湊的插件仍需更換註冊／查詢適配及審核新 DLL。先用獨立示範提供者驗證，然後在各插件專案適配；本輪不修改 BedrockTool 或其他插件。
- 不額外匯出四個 `MMB_Workspace*V1` 別名來宣稱直接相容。若未來需要 shim，必須另列明確提供者與測試範圍。
- 已交付 local3 及目前 PR #2 不直接混入未驗收的工作頁改動。先獨立本地功能分支，完成測試版和人工驗收，再提交獨立 PR；不另外開新 Issue 或通知上游，除非使用者要求。
- Windows 為首輪交付平台；公用 core／header 應能跨平台編譯。既有 PR 的 Android 平台隔離缺口仍須處理，不以 Windows 測試宣稱 Android 已適配。

## 驗收

1. 用獨立示範提供者展示所有七類控件及至少兩個分頁；重繪／開頁／失焦不能增加命令數。另驗證無宣告／false 不顯示、有合法宣告才顯示、未就緒時停用，以及原有 Configure／open 不受影響。
2. 真實 consumer／broker 測試短緩衝、滿佇列、舊 session／revision、重複請求、錯誤結果及完整邊界；v1／v2 舊 API client 與現有設定交易保留。
3. 測試工作頁點擊、捲動、輸入法提交、比例／小視窗、取消危險確認、返回、焦點恢復與場景失效。測試同插件多個內嵌／外部入口、只含外部入口的宣告、未知目標、交接成功／失敗／逾時、不重送、不穿透，以及明確返回時的 origin/session 驗證。
4. 圖像更新及 renderer/device 重建後內容正確，沒有過期 GPU 資源或外部指標。
5. Debug／Release、套件匯出與 SHA-256、既有原生四版本映像檢查通過。交付測試 DLL、大小、雜湊及步驟；不自動安裝或啟動遊戲。
6. 人工驗收後才與實際插件聯合測試。一般設定的既有插件不必全體重編；工作頁提供者逐項適配、逐項驗收。

## 已核對來源

- 現有 ModMenuBE `include/modmenu/ModMenuApi.h`、`src/api/ExportedApi.cpp`、`src/core/ConfigRegistry.h`。
- ModMenuBedrock 的 `integration/WORKSPACES.md`、`integration/sdk/modmenu_workspace_v1.h`、`Workspace.h`／`Workspace.cpp`：只作協定與仲介來源，保留 MIT 出處。
- BedrockTool 的 `docs/inprocess-dll/native-contract.md` 與本輪 external-operation UI 計畫：有舊 dependency pin 及另一條獨立介面工作，需由其專案處理遷移，不能在這裡直接覆寫。
