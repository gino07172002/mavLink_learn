# 把現階段的 C++ MAVLink 範例接入 QGroundControl

本章是操作教學；[實際串接紀錄](QGroundControl串接.md) 另保存 2026-10-26 的觀察與限制。先完成這一章，再進入真正 PX4 SITL 的練習。

## 1. 這次要接的是什麼？

我們讓 `lab04_bridge.exe` 扮演最小飛控，讓 QGroundControl（以下簡稱 QGC）扮演地面站。bridge 使用官方 MAVLink C headers，在本機 UDP 上傳送 HEARTBEAT、ATTITUDE 與 LOCAL_POSITION_NED；QGC 負責解析與顯示。

```text
教學 Commander / 運動模型
        ↓ 讀取狀態
lab04_bridge（本地 UDP 14560）
        ↓ MAVLink 封包 → 127.0.0.1:14550
QGroundControl（本地 UDP 14550）
        ↓ MAVLink Inspector / 即時曲線
```

反方向，QGC 會向 bridge 請求版本、訊息或任務資料。能收到這些請求，可以證明回傳路徑存在；bridge 是否實作這些功能，則是另一個問題。

這一章的驗收是「地面站辨識載具、資料持續更新」。這個小模型還不具備 PX4 的估測器、參數服務、任務服務與完整 preflight，因此不以 QGC 顯示 Ready 或能起飛作為驗收。

## 2. 準備檔案與建立程式

這台電腦的專案在 `D:\mavLink_learn`，QGC 已安裝於 `D:\QGroundControl\bin\QGroundControl.exe`，實測版本為 5.1.2。編譯需要 CMake、Ninja 與 C++17 編譯器；目前使用 w64devkit GCC。

PowerShell 先進到專案，再建立程式：

```powershell
Set-Location D:\mavLink_learn
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 1
```

應該產生 `build\lab04_bridge.exe` 等執行檔。`--parallel 1` 用來降低記憶體峰值；本機曾在預設平行編譯時遇到記憶體不足。

可先確認原有檢查通過：

```powershell
ctest --test-dir build --output-on-failure
```

這些自動檢查不需要 QGC。若已開啟 QGC，它占用 14550，而自動整合測試另選臨時埠，仍可獨立執行。

## 3. 先理解兩個 UDP 埠

| 程式 | 在哪個本機埠收資料 | 把資料送去哪裡 |
| --- | --- | --- |
| bridge | 14560 | `127.0.0.1:14550` |
| QGC | 14550 | 根據收到資料的來源，回到 bridge 的 14560 |

`127.0.0.1` 指同一台電腦。14560 和 14550 是傳輸層的埠，與 MAVLink 的 system ID、component ID 不同。本 bridge 在封包裡使用 `(sysid=1, compid=1)`。

這個例子是一個固定 UDP 對端。不要同時開預設設定的 Lab 01／02／03，因為它們也會 bind 14550，與 QGC 衝突。若想同時用 QGC 和 C++ client 觀察同一個載具，需要另一條 MAVLink instance 或 router；單純再開一個程式不能自動共用資料。

## 4. 最快啟動方式

在專案根目錄執行：

```powershell
.\scripts\start-qgc-demo.ps1
```

腳本會檢查 bridge 執行檔與 QGC 安裝路徑，啟動背景 bridge，再開啟可見的 QGC。預設 bridge 持續一小時，到時自行結束。已經在跑的 QGC 會保留；14560 若已是本教材 bridge，會重用，其原先設定與期限不會被重設。

若 QGC 安裝路徑不同，可以指定：

```powershell
.\scripts\start-qgc-demo.ps1 -QgcExe 'C:\Program Files\QGroundControl\bin\QGroundControl.exe' -DurationSeconds 600
```

如果原來的 QGC 尚未成功顯示視窗，請先自行關閉那個實例再重跑；腳本不會擅自終止既有程式。若系統阻擋 PowerShell 腳本，仍可使用下一節手動啟動；本教材不要求你更改永久系統執行政策。

## 5. 手動啟動：看清楚每一步

### 終端機 A：啟動 bridge

```powershell
Set-Location D:\mavLink_learn
.\build\lab04_bridge.exe normal 14560 127.0.0.1 14550 600
```

參數依序是模式、本地埠、對端 IP、對端埠、執行秒數。應立即看見：

```text
BRIDGE_READY port=14560 mode=normal
```

### 終端機 B：啟動 QGC

本機一般啟動與既有 `-swrast` 捷徑曾出現 OpenGL 初始化失敗；成功的方法是讓 QGC 子行程使用 Qt Quick 軟體繪圖：

```powershell
$qgcPreviousBackend = $env:QT_QUICK_BACKEND
try {
    $env:QT_QUICK_BACKEND = 'software'
    Start-Process -FilePath 'D:\QGroundControl\bin\QGroundControl.exe' -WorkingDirectory 'D:\QGroundControl\bin' -WindowStyle Normal
} finally {
    $env:QT_QUICK_BACKEND = $qgcPreviousBackend
}
```

這只影響新開 QGC 繼承的環境，沒有修改永久系統設定。其他電腦若正常繪圖，可直接開 QGC。Qt 的對應說明見 [Scene Graph Adaptations](https://doc.qt.io/qt-6/qtquick-visualcanvas-adaptations.html)。

QGC 使用 UDP AutoConnect 收到 HEARTBEAT 後，應辨識到一台 Quadrotor。若開啟多台 QGC 或另一個程式占用 14550，先解決埠衝突。

## 6. 在畫面確認資料真的在動

1. 點 QGC 左上角圖示，開啟主選單。
2. 選 **Analyze**。
3. 選左側 **MAVLink Inspector**。
4. 左側應出現 **ATTITUDE**、**HEARTBEAT**、**LOCAL_POSITION_NED**。
5. 點 ATTITUDE，確認 Component 是 1，Count 持續增加，`time_boot_ms` 不斷前進。
6. 觀察 roll 值隨時間變化；pitch 約為 0.05 rad、yaw 約為 0.2 rad，因為模型刻意使用這些合成值。
7. 勾選 roll 右側的 **Plot 1**，等待曲線出現。此環境第一次建立圖表有延遲。

Inspector 顯示 ATTITUDE 的原始欄位是 rad；Lab 01 終端機輸出才轉成 degree。不要直接拿兩邊數字比較大小，應先換單位。Inspector 操作背景可參考 [QGC MAVLink Inspector](https://docs.qgroundcontrol.com/Stable_V5.0/en/qgc-user-guide/analyze_view/mavlink_inspector.html)。

本機實測 HEARTBEAT 約 3.2～4.0 Hz，ATTITUDE 與 LOCAL_POSITION_NED 約 16 Hz。程式的目標資料週期是 50 ms，但 Windows 等待粒度與排程會影響實際頻率。驗收以計數與資料持續更新為主，不把目標 20 Hz 當成實測保證。

## 7. 哪些提示屬於目前功能缺口？

| 畫面或 log | 在本練習裡的意思 |
| --- | --- |
| `Not Ready` | 最小 bridge 沒有提供完整飛控就緒條件，不能拿它做起飛判斷 |
| `Custom:0x0` | generic autopilot 的簡化 custom mode，沒有完整模式名稱對應 |
| 版本／metadata request 重試失敗 | bridge 尚未支援對應資訊服務 |
| `Mission request list failed` | QGC 自動讀取任務，但 bridge 未實作 Mission protocol |
| 地圖沒有移動中的 GPS 載具 | 目前只送本地位置，沒有全球 GPS 位置；底圖不是 bridge 的 GPS 證據 |

這些提示可以收起，回到 Inspector 繼續看遙測；不要透過亂改安全參數或把 autopilot type 改成 PX4，來假裝已完成真正飛控整合。

## 8. 找不到資料時，照這個順序查

| 問題 | 檢查動作 |
| --- | --- |
| bridge 一啟動就退出 | 看終端機錯誤，先確認 14560 沒被占用 |
| QGC 沒出現主視窗 | 檢查 OpenGL 錯誤，試本章成功使用的軟體繪圖方法 |
| QGC 找不到載具 | 確認 bridge 仍在執行、送往 127.0.0.1:14550、QGC UDP AutoConnect 有啟用 |
| Inspector 有資料名稱，但 Count 不增加 | 看 bridge 是否已到執行期限；等至少數秒再比較 |
| 看到 HEARTBEAT，沒看到姿態 | 確認開的是本教材 normal bridge，查看 ATTITUDE 是否有傳送 |
| 回傳請求收不到 | 本 bridge 嚴格比對對端 IP:port，檢查 QGC 是否從 14550 發送 |

PowerShell 可用以下指令查看埠的擁有者：

```powershell
Get-NetUDPEndpoint -LocalPort 14550,14560 | Select-Object LocalAddress,LocalPort,OwningProcess
```

若使用啟動腳本，bridge log 在 `build\qgc-bridge.log`；成功啟動的 QGC 診斷 log 在 `build\qgc-software-error.log`。QGC 發出的 REQUEST_MESSAGE 會在 bridge 印成 `VEHICLE_COMMAND id=512`；這是雙向通訊的證據，但不是該請求功能已實作的證據。

## 9. 本章完成檢查

- [ ] 能解釋 QGC 14550 與 bridge 14560 的收送方向。
- [ ] QGC Inspector 收到 component 1 的三種遙測。
- [ ] ATTITUDE 的 Count、time_boot_ms 與 roll 持續改變。
- [ ] 能顯示 roll 曲線，並說明 rad 與 degree 的差別。
- [ ] 知道 Not Ready、Mission 失敗不等同 UDP 斷線。
- [ ] 知道這裡接的是教學模型，後續真正 PX4 要另做 SITL 驗證。

停止時，前景 bridge 可按 Ctrl+C；背景 bridge 可等期限結束，或在確認為本次啟動的行程後自行停止。QGC 可正常關閉。接下來的 PX4 SITL 會使用獨立工作目錄與埠，避免把兩個 sysid=1 的載具混在同一條 QGC 連線。
