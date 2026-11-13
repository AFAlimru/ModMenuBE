# 安卓版 ModMenuBE（原生 .so 模組）研究報告

日期：2026-11-10

## 結論

可以做，而且技術路線很明確：做成一個標準的 LeviLauncher 安卓原生模組（`.so` 檔，`type: preload-native`）。

- **繪製**：hook `eglSwapBuffers`，在每一幀結束時用 ImGui 的 OpenGL ES 3 後端畫介面。
- **輸入**：用 Preloader 的輸入 API 接收觸控、按鍵和輸入法文字，選單開著時把事件攔下來，遊戲就收不到。
- **程式碼**：大部分沿用 Windows 版的 C++ 核心。

不需要修改啟動器，也不用另外安裝 APK，放進官方 LeviLauncher 就能用。

## 官方文件與資料來源

- 安卓原生模組開發指南：https://docs.levimc.org/LeviLamina/developer_guides/android/
  - 子頁面：Mod、Mod Menu、Config、Input、Hook、Signature、Patch API
- Preloader SDK 原始碼：https://github.com/LiteLDev/preloader-android （版本 0.2.2 之後，commit `cc08b40`）
- 官方模組範本：https://github.com/QYCottage/levilauncher-android-mod-template （CMake、NDK 28.2，產出 `.levipack`）
- 參考模組：https://github.com/QYCottage/BedrockTools （已經在用 `eglSwapBuffers` hook 畫 GLES）
- LeviLaunchroid 的 `examples/full-cpp-mod`、`examples/mod-menu-api`

## 各項能力（確認過原始碼）

| 需求 | 做法 | 依據 |
| --- | --- | --- |
| 載入與生命週期 | `PL_REGISTER_MOD`，提供 `load/enable/disable/unload`。`enable()` 裡註冊 hook、輸入和 UI | 官方 Mod API |
| 畫面 | `pl::memory::hook` hook `libEGL.so` 的 `eglSwapBuffers`，在呼叫原函式之前畫 ImGui | BedrockTools `GameHooks.cpp` 就是這樣做 |
| 渲染後端 | 安卓版基岩版用 OpenGL ES，包括 Vibrant Visuals | RenderDragon 資料；玩家回報部分新 GPU 有 Vulkan 白名單需求（風險，見下方） |
| 觸控 | `pl::input::registerTouchCallback`。在 UI 執行緒派送，座標是遊戲畫面的像素；回傳 `true` 遊戲就收不到（視角不會動） | `MinecraftActivity.dispatchTouchEvent` → `PreloaderInput.onTouch` |
| 文字輸入 | `registerTextInputCallback` 接收輸入法文字，`showKeyboard()`／`hideKeyboard()` 開關軟鍵盤，可以打中文搜尋 | `pl/Input.hpp`、`InputJni.cpp` |
| 返回鍵 | `registerKeyCallback`，`keyCode == 4`（BACK）時關閉選單並回傳 `true` | 官方 Input API |
| 入口 | `pl::modmenu::ButtonBuilder` 註冊一個懸浮按鈕；或在內建 Mod Menu 註冊「ModMenu」模組，用 `onToggle` 開啟 | 官方 Mod Menu API |
| 列出其他模組 | 沒有官方 API。但 `getModDir()` 指向**原始**模組資料夾，它的上一層就是 `mods/`，可以直接掃各模組的 `manifest.json` | `ModManifest.cpp` `createRuntimeModInfo` 使用 `sourceModDirectory` |
| 啟用／停用 | 啟動器把狀態存在 `mods/mods_config.json`（啟用表與載入順序），下次啟動才生效 | `ModManager.java`（啟動器私有格式，有相容風險） |
| 網路（README、更新檢查） | `getJavaVM()` 拿到 JavaVM，用 JNI 呼叫 Java 的 `HttpURLConnection`，不用自帶 libcurl | `pl/Mod.hpp` |
| 開連結 | 透過 JNI 送出 `Intent.ACTION_VIEW` | 同上 |
| 圖片 | 用 stb_image 解碼 PNG（BedrockTools 也用），上傳成 GL 貼圖 | — |
| 中文字型 | 讀系統字型 `/system/fonts/NotoSansCJK-Regular.ttc` | — |
| 遊戲內資料 | 本功能不需要，所以不用簽名掃描，比較不受遊戲版本影響 | — |

### 需要特別處理的執行緒問題

- 觸控和按鍵在 **UI 執行緒**，畫面在**渲染執行緒**（`eglSwapBuffers`）。
- 要沿用 Windows 版的做法：輸入事件先排進佇列，在渲染執行緒交給 ImGui。
- ImGui 的 OpenGL 3 後端會自己保存並還原大部分 GL 狀態，但還是要測試它跟遊戲的 RenderDragon 以及 BedrockTools 的 hook 疊在一起時有沒有問題。

## 程式碼重用計畫

Windows 版程式碼分成兩類：

**可以直接沿用（平台無關）**
- `ModRegistry`（模組掃描）
- `Adapter`（`modmenu.json`）
- `ConfigRegistry`（設定頁 API）
- `JsonConfigDoc`
- `LipCatalog`（改用安卓的 LeviModHub 目錄）
- `UpdateChecker`
- Markdown 解析
- `I18n`
- OreUI 主題與所有畫面（`ModMenuScreen` 等）

**需要改成平台層**

| Windows | 安卓 |
| --- | --- |
| `Overlay`（DXGI + D3D11On12 + WndProc） | `EglOverlay`（`eglSwapBuffers` + ImGui GLES3）＋ `pl::input` 回呼 |
| `Http`（WinHTTP） | JNI + `HttpURLConnection` |
| `IconCache`（WIC） | stb_image |
| `Paths`（`GetModuleFileName`） | `getSelf().getModDir()` |
| `Log` | `getSelf().getLogger()` |
| `GameInputGuard`（PreLoader `MouseDevice::feed`） | 不需要：觸控回呼回傳 `true` 就擋住了 |
| `ModOpener`（`SendInput` 按熱鍵） | 改成呼叫目標模組註冊的 Mod Menu 模組，或 Android `KeyEvent`（需要研究） |

建議把 repo 整理成 `core/` + `platform/windows/` + `platform/android/`。Windows 用 xmake，安卓用官方範本的 CMake（或 xmake，BedrockTools 用的是 xmake）。

## 風險

1. **Vulkan**：如果某些裝置或之後的版本改用 Vulkan，`eglSwapBuffers` 不會被呼叫，選單就畫不出來。對策：偵測到沒有 EGL 幀時，退回只用 `pl::modmenu` 的懸浮按鈕和啟動器內建選單顯示基本資訊。
2. **與啟動器內建 Mod Menu 共存**：兩個選單同時開會搶觸控。需要在內建選單開著時隱藏我們的選單。內建選單的狀態沒有公開 API，可能要用 `requestCloseMenu()` 主動關掉它。
3. **`mods_config.json` 是私有格式**：啟動器改版就可能壞。可以只做唯讀，啟用／停用改成提示「請在啟動器裡切換」，或者向官方提議一個公開 API。
4. **測試環境**：遊戲內功能需要能正常啟動的正版 Minecraft（從 Google Play 安裝）。x86 模擬器靠 ARM 轉譯執行，GL 行為可能跟真機不同，最後要在 ARM 實機上確認。
5. **輸入法**：`TextInputEvent` 只有已確認送出的文字，看不到組字過程，搜尋框會在選字完成後才更新。

## 建議的第一步（最小可行版本）

1. 用官方範本建立模組專案，接上 preloader 0.2.2 和 ImGui（OpenGL ES 3 + 自訂觸控輸入）。
2. hook `eglSwapBuffers`，畫一個測試視窗，確認在模擬器和實機上都看得到、不會讓遊戲當掉。
3. 加上懸浮按鈕開關選單、觸控攔截和返回鍵關閉。
4. 接上沿用的核心：模組列表、詳細資訊、README（JNI HTTP）。
5. 之後再補設定頁、`modmenu.json` 和更新檢查。
