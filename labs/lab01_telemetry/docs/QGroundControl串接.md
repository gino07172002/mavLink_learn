# QGroundControl 實際串接紀錄

日期：2026-10-26，Asia/Taipei。

## 本次結果

已開啟這台電腦既有的 **QGroundControl 5.1.2（64-bit）**，連上本教材的 C++ `lab04_bridge`，並在 Analyze → MAVLink Inspector 直接觀察到：

| 訊息 | 實際畫面觀察 |
| --- | --- |
| HEARTBEAT | system 1、component 1，約 3.2～4.0 Hz |
| ATTITUDE (30) | 約 16.0 Hz；count 與 time_boot_ms 持續增加，roll 隨時間變動 |
| LOCAL_POSITION_NED | 約 16.0 Hz；初始 x/y/z 為零 |

例如兩次 ATTITUDE 畫面 count 從 4 增加到 190，後續超過 600；roll、time_boot_ms 也隨之變動。這不是只看見載具圖示，而是確認 parser 與訊息更新。

程式目標資料週期為 50 ms，Windows 實際 UI 測得約 16 Hz，沒有把目標 20 Hz 寫成實測值。程式仍以 `now + interval` 排程，實際頻率會受到 receive 等待、Windows 計時粒度與負載影響。

雙向通訊也有證據：bridge 收到 QGC 發出的 `COMMAND_LONG command=512`（REQUEST_MESSAGE），QGC log 則出現：

```text
Adding new vehicle link:vehicleId:componentId:vehicleFirmwareType:vehicleType
"UDP Link (AutoConnect)" 1 1 0 2
```

## 本次連線配置

```text
QGroundControl     bind UDP 0.0.0.0:14550
lab04_bridge       bind UDP 0.0.0.0:14560
bridge destination     127.0.0.1:14550
QGC return destination 127.0.0.1:14560
vehicle identity       sysid=1, compid=1
autopilot type         MAV_AUTOPILOT_GENERIC
vehicle type           MAV_TYPE_QUADROTOR
```

bridge 使用既有的 normal 模式，沒有為了讓 QGC 畫面通過而冒充 PX4 或新增假的完整功能。

## 如何再次啟動

在專案根目錄的 PowerShell 執行：

```powershell
.\scripts\start-qgc-demo.ps1
```

預設使用這台電腦的 `D:\QGroundControl\bin\QGroundControl.exe`，bridge 執行一小時。若安裝位置不同：

```powershell
.\scripts\start-qgc-demo.ps1 -QgcExe 'C:\Program Files\QGroundControl\bin\QGroundControl.exe' -DurationSeconds 600
```

腳本會保留既有 QGC 行程；如果已經有教材 bridge 占用 14560，會重用它，其原始執行時間與對端設定仍有效。它不會自動關閉或終止既有程式。若另一個程式占用 14560，會回報錯誤。

不要同時執行預設埠的 Lab 01 / 02 / 03，因為 14550 已被 QGC 使用。本次沒有加入多 client router；要讓 QGC 與 C++ client 同時接同一個模型，需要額外設計 fan-out / routing，不能直接搶用同一埠。

進入 QGC 左上角選單 → Analyze → MAVLink Inspector，點選 ATTITUDE，可查看欄位與 Actual Rate。本次也勾選 roll 的 Plot 1，觀察到隨時間變化的綠色曲線；結束時保留這個畫面。軟體繪圖初次顯示圖表有延遲，其他視覺功能仍可能有渲染限制。

## 啟動問題與處理

一般啟動及安裝時既有的 GPU Safe Mode（`-swrast`）都未成功顯示可用主畫面，後者出現：

```text
Failed to initialize graphics backend for OpenGL.
```

本次改在啟動 QGC 的子行程環境使用 `QT_QUICK_BACKEND=software` 後正常顯示。此方式使用 Qt Quick 的 software adaptation，與僅指定 OpenGL 軟體 rasterizer 不同，見 [Qt Scene Graph Adaptations](https://doc.qt.io/qt-6/qtquick-visualcanvas-adaptations.html)。腳本會在啟動後還原呼叫端環境值，沒有更動永久系統環境變數。

本次也使用 QGC 的命令列 logging 保存診斷，相關用法見 [QGC Command Line Options](https://docs.qgroundcontrol.com/master/en/qgc-dev-guide/command_line_options.html)。log 放在 `build/`：

- `qgc-bridge.log`、`qgc-bridge-error.log`：教學端。
- `qgc-software-error.log`、`qgc-software.log`：成功開啟的 QGC。
- `qgc-startup-error.log`：先前 OpenGL 啟動的診斷。

## 畫面上的限制如何解讀

QGC 已連上，但主畫面顯示 **Not Ready**、模式為 **Custom:0x0**。初始化 log 也有 autopilot version request retries exhausted、metadata 讀取失敗；稍後自動讀取任務會提示 `Mission request list failed, maximum retries exceeded`，本次已收起提示。這個 bridge 只有最小訊息集合，尚未實作 AUTOPILOT_VERSION、Parameter / Component Metadata、Mission、GPS 或完整飛控健康檢查。

因此這次證明的是：**QGC 能辨識這個 MAVLink 系統，接收並解碼即時姿態／位置資料，且能向 bridge 發送請求。** 沒有宣稱參數下載、QGC 解鎖按鈕、起飛或任務上傳已驗證。地圖底圖的位置沿用 QGC 原有設定，bridge 沒有提供全球 GPS 位置。

完成操作後，QGC 留在 MAVLink Inspector，bridge 持續執行，方便直接查看。bridge 執行滿設定的時間後會自行結束。
