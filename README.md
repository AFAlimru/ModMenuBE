# ModMenuBE

[繁體中文](#繁體中文) · [English](#english)

Minecraft 基岩版（Windows GDK）的模組選單。概念參照 Java 版的 [ModMenu](https://github.com/TerraformersMC/ModMenu)，介面採用 OreUI 風格，給用 [LeviLauncher](https://github.com/LiteLDev/LeviLauncher) 或 LeviLamina 的玩家使用。

---

## 繁體中文

### 功能

- 在遊戲中任何畫面按 **F8**（可改）開關選單；選單開著時，視角、點擊和移動不會傳給遊戲。
- 列出 `mods/` 裡的所有模組：LeviLamina 模組（`native`）和 LeviLauncher / PreLoader 載入的 `preload-native` 模組。
- 搜尋、排序、顯示／隱藏函式庫、父子分組，以及徽章：函式庫、客戶端、前置載入、LeviLamina、已停用、未載入、可更新。
- 詳細資訊：頭像、版本、作者、說明、連結、授權、載入狀態、資料夾，以及 GitHub 上的 README。
- 啟用／停用模組：跟 LeviLauncher 一樣把 `manifest.json` 改名為 `manifest.json.close`，重新啟動遊戲後生效。
- 設定畫面：
  1. 模組在 `modmenu.json` 裡描述設定檔，ModMenuBE 會照描述顯示多語系標籤、分組、滑桿和下拉選單。
  2. 模組在程式裡透過 C API 註冊自己的設定頁。
  3. 都沒有時，顯示 `config/*.json` 的通用編輯器。
- 更新檢查：lip、GitHub Releases、CurseForge，或自訂 JSON 網址。
- 頭像、說明、授權和 README 會自動從 [LeviMC 模組目錄](https://levimc.org/mods/)（lip 使用的目錄）補上。

沒有 LeviLamina 也能使用（ModMenuBE 是 `preload-native` 模組），只是遊戲視角在選單開著時不會鎖定（這個功能需要 LeviLamina 的 PreLoader）。

### 安裝

在 LeviLauncher 的模組瀏覽器或 [LeviMC 模組目錄](https://levimc.org/mods/) 搜尋 ModMenuBE 安裝（會一併裝上它需要的 LeviLamina）。

手動安裝：從 [Releases](https://github.com/AFAlimru/ModMenuBE/releases) 下載 `ModMenuBE-client-windows-x64.zip`，解壓到實例的 `mods/ModMenuBE/`（裡面會有 `ModMenuBE.dll`、`manifest.json`、`adapters/`），例如：

```
%LOCALAPPDATA%\LeviLauncher\Instances\<實例>\mods\ModMenuBE\
```

ModMenuBE 自己的設定可以在選單裡的「ModMenuBE → 設定」修改，也存在 `mods/ModMenuBE/config/config.json`。紀錄檔在 `mods/ModMenuBE/logs/latest.log`。

### 讓你的模組支援 ModMenuBE

#### 和 Java 版 ModMenu 的差別

Java 版的「設定」按鈕會直接開啟模組自己的設定畫面。那是因為 Fabric 模組會實作 `ModMenuApi`，把自己的畫面交給 ModMenu。

基岩版沒有這種共通的介面標準：每個模組的設定介面放在哪裡、怎麼開啟（熱鍵、聊天指令、自己畫的 OreUI 或 ImGui 選單），ModMenuBE 都無從得知。所以**模組要先適配，ModMenuBE 才知道「設定」按鈕該做什麼**。

| 模組做了什麼 | 按下「設定」時 |
| --- | --- |
| 用 C API 的 `setExternalScreen` 註冊開啟函式 | 關閉 ModMenuBE，**開啟模組自己的選單**（最接近 Java 版） |
| 用 C API 註冊設定頁 | 開啟 ModMenuBE 畫的設定頁 |
| `modmenu.json` 寫了 `open`（模組的開啟熱鍵） | 關閉 ModMenuBE，**替玩家按下模組的熱鍵**，開啟模組自己的選單；JSON 設定檔改由「設定檔」按鈕開啟 |
| 提供 `modmenu.json`（或 `adapters/` 裡有社群適配檔） | 照描述編輯模組的 JSON 設定檔 |
| 什麼都沒做 | 退回通用編輯器，直接列出 `config/*.json` 的內容 |
| `modmenu.json` 寫了 `"config": false` | 不顯示「設定」按鈕 |

已經有自己設定介面的模組（例如 LHolo 的 OreUI 選單），有兩種做法讓「設定」按鈕直接打開它：

- **不用改程式**：在 `modmenu.json` 寫 `open`，告訴 ModMenuBE 模組用哪個熱鍵開啟選單（見第 2 節）。
- **改幾行程式**：用第 3 節的 `setExternalScreen`，最可靠，不受按鍵綁定影響。

有三種適配方式，由簡到繁。都不需要連結 ModMenuBE，沒安裝 ModMenuBE 時你的模組照常運作。

#### 1. 只寫 manifest（模組資訊）

在 `manifest.json` 加一個 `modmenu` 物件（全部選填）：

```json
{
    "name": "MyMod",
    "entry": "MyMod.dll",
    "version": "1.2.0",
    "type": "native",
    "description": "一行或多行說明",
    "author": "Me",
    "modmenu": {
        "icon": "icon.png",
        "authors": ["Me", "Friend"],
        "license": "MIT",
        "links": {
            "homepage": "https://example.com",
            "source": "https://github.com/me/MyMod",
            "issues": "https://github.com/me/MyMod/issues"
        },
        "badges": ["library"],
        "parent": "MyModpack",
        "lip": "github.com/me/MyMod",
        "update": { "github": "me/MyMod" }
    }
}
```

| 欄位 | 說明 |
| --- | --- |
| `icon` | 模組資料夾內的 PNG。沒寫時會找 `icon.png`，再找不到就用 LeviMC 目錄的圖示或 GitHub 頭像。 |
| `badges` | `library`（預設隱藏）、`deprecated`、`client`、`modpack`。 |
| `parent` | 在列表中把這個模組放在另一個模組底下。 |
| `lip` | lip 識別碼，用來從 LeviMC 目錄取得 README 和版本。不寫時用模組名稱比對。 |
| `update` | `{"github": "owner/repo"}`、`{"lip": "github.com/owner/repo"}`、`{"curseforge": 123456}`、`{"url": "https://..."}`（回傳 `{"version": "...", "url": "..."}`），或 `false` 關閉。加上 `"sameMinor": true` 時，只有相同 `major.minor` 的新版才算可更新，適合版本跟著遊戲走的模組（例如 `26.60.x`）。 |

已經發佈在 lip 上的模組，什麼都不寫也會自動取得頭像、說明、授權和 README。

#### 2. 寫 `modmenu.json`（設定畫面，建議）

在模組資料夾放一個 `modmenu.json`，描述要讓玩家調整哪些設定。ModMenuBE 會直接編輯你的 JSON 設定檔，你的程式**不用改任何一行**，維護也只要改這一個檔案。

```json
{
    "$schema": "https://raw.githubusercontent.com/AFAlimru/ModMenuBE/main/schema/modmenu.schema.json",
    "config": {
        "files": [
            {
                "path": "config/config.json",
                "title": { "zh_TW": "設定", "en_US": "Settings" },
                "hidden": ["version", "internal"],
                "sections": [
                    {
                        "title": { "zh_TW": "顯示", "en_US": "Display" },
                        "options": [
                            {
                                "key": "hud.enabled",
                                "label": { "zh_TW": "顯示 HUD", "en_US": "Show HUD" },
                                "description": { "zh_TW": "在畫面左上角顯示資訊。", "en_US": "Info in the top-left corner." }
                            },
                            {
                                "key": "hud.opacityPercent",
                                "label": { "zh_TW": "不透明度", "en_US": "Opacity" },
                                "min": 0, "max": 100, "step": 5, "format": "%d%%"
                            },
                            {
                                "key": "language",
                                "label": { "zh_TW": "語言", "en_US": "Language" },
                                "choices": [
                                    { "value": "zh_TW", "label": "繁體中文" },
                                    { "value": "en_US", "label": "English" }
                                ]
                            },
                            "advanced"
                        ]
                    }
                ]
            }
        ]
    }
}
```

規則：

- `key` 是設定檔裡的路徑，用 `.` 分隔（`hud.enabled`），也可以寫 JSON pointer（`/hud/enabled`）。指向物件時，會列出該物件底下所有的值。
- 控制項依值的型別決定：布林值顯示開關，數字有 `min`/`max` 時顯示滑桿（否則是輸入框），有 `choices` 時顯示下拉選單，字串顯示輸入框。
- `label`、`description`、`title` 可以是字串，也可以是語言物件（`zh_TW`、`zh_CN`、`en_US`……）。找不到目前語言時，會依序退回其他中文、`en_US`。沒寫 `label` 時，會從 key 自動產生（`fillOpacityPercent` → 「Fill opacity percent」）。
- `hidden`：不顯示的 key，寫物件路徑會隱藏整個物件。
- `listedOnly: true`：只顯示 `sections` 列出的項目。預設為 `false`，沒列出的值會出現在「其他」。
- `restartRequired`：預設為 `true`，會提示玩家儲存後重新啟動遊戲。
- `readOnly: true`：只顯示，不能修改。
- `"config": false`：不提供設定按鈕（例如你的模組有自己的設定介面）。
- `modmenu.json` 也可以放第 1 節的模組資訊欄位（`description`、`links`……），作用跟寫在 manifest 一樣。

##### 用熱鍵開啟模組自己的選單（`open`）

模組有自己的設定選單、而且可以用熱鍵開啟時，在 `modmenu.json` 加上 `open`。玩家按「設定」時，ModMenuBE 會先關閉自己的選單，再替玩家按下那組熱鍵：

```json
{
    "open": {
        "hotkey": { "file": "config/config.json", "key": "input.hotkeys.openMenu", "default": "Alt+M" },
        "label": { "zh_TW": "開啟 LHolo 選單", "en_US": "Open LHolo menu" },
        "delayMs": 150
    }
}
```

- **熱鍵通常可以讓玩家重新綁定**，所以建議用 `file` + `key` 指向模組設定檔裡存放綁定的位置。每次按下按鈕時都會重新讀取，玩家改過綁定也能跟著用。設定檔裡沒有這個 key 時，才用 `default`。
- 熱鍵是固定的、不能改時，可以直接寫字串：`"hotkey": "Alt+M"`。
- 玩家把綁定清空（值是 `null`）時，按鈕會提示「沒有綁定開啟選單的按鍵」，不會亂按。
- 支援的格式：用 `+` 連接修飾鍵和按鍵，可以有空格，大小寫不拘。
  - 修飾鍵：`Ctrl`、`Alt`、`Shift`、`Win`
  - 按鍵：`A`–`Z`、`0`–`9`、`F1`–`F24`、`Numpad0`–`Numpad9`、`Left`／`Up`、`PageUp`、`Esc`、`Space`、`Enter` 等
  - 滑鼠：`MouseMiddle`、`MouseSide1`、`MouseSide2`
  - 代碼：`VK_<十進位>`
- 按鍵是用 `SendInput` 送出的真實輸入。不論模組是從視窗訊息收按鍵，還是用 `GetAsyncKeyState` 輪詢，都收得到。
- 選單沒打開時，可以把 `delayMs`（預設 150 毫秒）調大一點，讓遊戲有時間在 ModMenuBE 關閉後重新取得焦點。
- 寫了 `open` 之後，`config` 描述的 JSON 設定檔還是可以用「設定檔」按鈕編輯。

##### 指令按鈕（`commands`）

模組靠聊天指令操作時（例如 Playback 的 `/record start`），可以列在 `commands`，詳細資訊頁會多一個「指令」區，每個指令一顆按鈕。按下後 ModMenuBE 會關閉、打開聊天欄（T 鍵）、幫玩家輸入指令並送出，所以要在世界中使用。目前只有 Windows 版支援。

```json
{
    "commands": [
        {
            "command": "record start",
            "label": { "en_US": "Start recording", "zh_TW": "開始錄影" },
            "description": { "en_US": "Starts or resumes recording.", "zh_TW": "開始或繼續錄影。" }
        },
        "playback version"
    ]
}
```

`command` 不用加開頭的 `/`；只寫字串時，按鈕文字就是指令本身。

**模組作者還沒提供 `modmenu.json` 怎麼辦？** 任何人都可以寫一份，放在 `mods/ModMenuBE/adapters/<模組名稱>.json`。模組資料夾裡的 `modmenu.json` 優先。ModMenuBE 目前內建 LHolo、Lamium、LeviLamina、Playback 的適配檔，歡迎發 PR 新增。

#### 3. 用 C API（在程式裡註冊設定頁）

設定不在 JSON 檔裡，或是需要即時生效時，可以用 `include/modmenu/ModMenuApi.h`。它是純 C ABI，透過 `GetProcAddress` 取得，不需要連結。C++ 可以用 header-only 的 `ModMenuApi.hpp`：

```cpp
#include <modmenu/ModMenuApi.hpp>

std::atomic<bool> gEnabled{true};
std::atomic<int>  gRange{16};

void registerModMenu() { // 在模組 enable 時呼叫
    auto screen = modmenu::Screen::begin("MyMod", "My Mod 設定");
    if (!screen) return; // 沒有安裝 ModMenuBE
    screen.category("general", "一般")
          .toggle("enabled", "啟用", [] { return gEnabled.load(); }, [](bool v) { gEnabled = v; })
          .slider("range", "範圍", 1, 64, [] { return int64_t(gRange); }, [](int64_t v) { gRange = int(v); })
          .choice("mode", "模式", {"簡單", "進階"}, [] { return int64_t(0); }, [](int64_t) {})
          .button("reset", "恢復預設", [] { /* ... */ })
          .onSave([] { /* 寫入你的設定檔 */ });
}
```

已經有自己設定介面的模組，可以讓「設定」按鈕改成打開你的介面：

```cpp
modmenu::setExternalScreen("MyMod", [] { openMyOwnMenu(); });
```

- 第一個參數是 `manifest.json` 的 `name`。
- get/set 回呼在**渲染執行緒**執行，請保持輕量並確保執行緒安全（atomic 或 mutex）。
- 回呼發生例外時，ModMenuBE 會停用該設定頁，不會讓遊戲崩潰。
- 用 API 註冊的設定頁，優先於 `modmenu.json` 和通用編輯器。

### 建置

需要 Visual Studio 2022 和 [xmake](https://xmake.io)。

```bash
xmake f -a x64 -m release -p windows -y
xmake -y
```

輸出在 `bin/ModMenuBE/`。`xmake build TestHost` 會建出一個 D3D12 測試視窗，不用開遊戲也能檢查介面：

```bash
build/windows/x64/release/TestHost.exe <mods\ModMenuBE\ModMenuBE.dll> --open [--mod LHolo --config]
```

### 目前限制

- 介面是用 ImGui 畫成 OreUI 風格。[Dear-OreUI](https://github.com/copper-lamp/Dear-OreUI) 目前只支援 LeviLamina 26.10.x，等它支援新版本後會加上真正的 OreUI 前端。
- 只支援 GDK（Win32 視窗）版本。
- 開啟選單時遊戲不會暫停。

---

## English

A mod menu for Minecraft Bedrock (Windows GDK), modelled on Java's [ModMenu](https://github.com/TerraformersMC/ModMenu) and drawn in the OreUI style, for [LeviLauncher](https://github.com/LiteLDev/LeviLauncher) and LeviLamina players.

### Features

- **F8** (configurable) opens the menu on any screen. While it is open, camera, clicks and movement do not reach the game.
- Lists every mod in `mods/`: LeviLamina mods (`native`) and `preload-native` mods.
- Search, sort, show/hide libraries, parent/child grouping, badges.
- Details: icon, version, authors, description, links, license, load state, folder, and the README from GitHub.
- Enable/disable: renames `manifest.json` to `manifest.json.close`, like LeviLauncher; applies after a restart.
- Config screens from `modmenu.json`, from the C API, or a generic `config/*.json` editor.
- Update checks through lip, GitHub Releases, CurseForge or a JSON URL.
- Icon, description, license and README are filled in from the [LeviMC mod catalogue](https://levimc.org/mods/).

### Installing

Search for ModMenuBE in LeviLauncher's mod browser or on [LeviMC Mods](https://levimc.org/mods/) (LeviLamina is installed with it). To install by hand, extract `ModMenuBE-client-windows-x64.zip` from [Releases](https://github.com/AFAlimru/ModMenuBE/releases) into the instance's `mods/ModMenuBE/` folder, e.g. `%LOCALAPPDATA%\LeviLauncher\Instances\<instance>\mods\ModMenuBE\`.

### Supporting ModMenuBE in your mod

On Java, ModMenu's Config button opens the mod's own screen because Fabric mods hand it over through `ModMenuApi`. Bedrock has no such convention: ModMenuBE cannot know where a mod's settings UI lives or how to open it (a hotkey, a chat command, its own OreUI or ImGui menu). A mod has to tell it:

| What the mod provides | What the Config button does |
| --- | --- |
| `setExternalScreen` through the C API | closes ModMenuBE and **opens the mod's own menu** (closest to Java) |
| a config screen through the C API | opens that screen, drawn by ModMenuBE |
| `modmenu.json` (or a community adapter in `adapters/`) | edits the mod's JSON config as described |
| nothing | falls back to a generic editor for `config/*.json` |
| `"config": false` in `modmenu.json` | no Config button |
| `open` in `modmenu.json` (the mod's menu hotkey) | closes ModMenuBE and **presses the mod's hotkey** for the player |

Mods driven by chat commands can list them under `commands` in `modmenu.json` (each `{ "command": "record start", "label": …, "description": … }` or just a string, without the leading `/`). The details page then shows a Commands row; a button closes ModMenuBE, opens the chat (T), types the command and sends it, so it works in a world (Windows only). A Playback adapter ships with ModMenuBE.

Mods that already have a settings UI can either add `open` to `modmenu.json` (no code; point `hotkey.file`/`hotkey.key` at where the player's binding is stored, since bindings can change) or call `setExternalScreen` (a few lines, independent of key bindings).

1. **Manifest metadata:** add a `modmenu` object to `manifest.json` (see the Chinese section above for every field). Mods published on lip get the icon, description, license and README without it.
2. **`modmenu.json` (recommended):** describe your JSON config file and ModMenuBE shows it with localized labels, sections, sliders and dropdowns, with no code changes on your side. See the example above and [`schema/modmenu.schema.json`](schema/modmenu.schema.json). Anyone can write one for a mod that has none and put it at `mods/ModMenuBE/adapters/<ModName>.json`.
3. **C API:** `include/modmenu/ModMenuApi.h` (plain C ABI, looked up with `GetProcAddress`), or the header-only `ModMenuApi.hpp`, for settings that live in code. Callbacks run on the render thread.

### Building

Visual Studio 2022 and xmake: `xmake f -a x64 -m release -p windows -y`, then `xmake -y`. Output in `bin/ModMenuBE/`.

## License

MIT
