# ModMenuBE local4 工作頁測試

這是獨立本地 Windows x64 候選版，未更新 PR #2，尚未遊戲內驗收。保留 local3 回退包。四個既有精確映像的離線檢查不等於四版本實機驗收。

1. 完全退出 Minecraft，備份目前 `mods/ModMenuBE`。更新 runtime ZIP 內的 `mods/ModMenuBE`，保留自己的設定。不要同時載入舊 ModMenuBedrock／LL 或重複 ModMenuBE。
2. 此輪先不加入尚未適配的 BedrockTool。現有共用 Hook 限制沿用 local3，26.60 新 MouseTweaks 仍需獨立配對適配，未因工作頁而放寬。
3. 另將可選 demo ZIP 中 `mods/ModMenuWorkspaceExample` 放入同一實例。它只有示範計數、進度、靜態像素及兩個自有測試視窗，不會登入、連網、讀寫玩家檔案。
4. 啟動後在主選單按 F10。普通插件若未宣告 workspace，不應新增工作頁入口；示範插件應有工作頁。進入後有「任務／預覽／開啟編輯器／開啟檢視器」四個按鈕。
5. 不點操作，只開頁／切頁／等幾秒／切至瀏覽器再回來，Handled commands 應維持 0。任務頁修改文字、切換開關；選清單會明確送出 choose 命令，點 Start 觀察進度及結果；Reset 先取消確認，再確認，核對只在明確操作時執行。
6. 預覽頁應顯示綠灰方格。縮小視窗、調整 UI 比例、滾動及中英文顯示，檢查沒有文字／按鈕重疊。Back／Escape 先回工作頁入口，再回插件詳情；從暫停選單測一次，返回仍應暫停。
7. 點「開啟編輯器」後放開所有按鍵：只應開一個插件自有視窗。按其 Return to Workspace，切回 Minecraft 應恢復原入口頁。再測另一個「檢視器」目標。關窗的 X 不應自動打開選單，可用 F10 明確重開。這是另一個原生視窗，外觀由示範插件控制。
8. 工作頁內切到瀏覽器後返回，原分頁／捲動及已完成輸入應保留；未完成輸入與危險確認取消。按住鍵返回不應誤觸。工作頁開著時離開原場景／進入世界，不得自動恢復舊頁。
9. 要測宣告規則，退出遊戲後備份 demo 的 `modmenu.json`，先改成 `workspace:false` 或移除 workspace 再重啟，入口應消失；恢復檔案後再測。不要在遊戲執行期間改 DLL。
10. 最後移除／停用示範插件，確認普通 Configure 的 Apply／Cancel、快捷鍵與材質包 Mods 入口沿用原行為。示範插件沒有持久設定需要遷移。

日誌仍在 `mods/ModMenuBE/logs/latest.log`。回報請提供遊戲版本、DLL 版本、哪個入口及具體操作；本包沒有安裝或啟動遊戲。實際插件的註冊、熱更新及 dependency pin 需要在其專案另做適配，不要移除保護。
