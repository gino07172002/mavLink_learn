# Lab 操作、練習與解答導讀

本文件涵蓋最初 Lab 01～04。新增 Lab 05～08 的操作與解答在 [PX4 SITL 與後續實作](PX4_SITL與後續實作.md)，包含真正飛控、原有 PX4-Lite 與進階服務。

本文件命令以專案根目錄的 PowerShell 為準。先依 [README](../README.md) 完成編譯。所有程式都是完整解答；建議先讀任務、自己試寫，再對照對應原始檔。

## 共用命令列與埠配置

```text
lab01_telemetry [local_port peer_ip peer_port duration_seconds]
lab02_command arm|disarm [local_port peer_ip peer_port duration_unused]
lab03_offboard [local_port peer_ip peer_port move_duration_seconds]
lab04_bridge [mode [local_port peer_ip peer_port duration_seconds]]
```

參數必須依順序提供，不能跳著填。client 預設是 `14550 127.0.0.1 14560 6`；bridge 預設是 `normal 14560 127.0.0.1 14550 30`。Lab 02 的最後一個 duration 不影響內建命令期限，只為共用參數格式而保留。

同一時間只執行一個 client，因為它們預設使用相同的本機埠。每次實驗前可重啟 bridge，讓 armed、offboard、位置等狀態回到初始值。

若 14550 已被 QGC 占用，換一組埠，例如：

```powershell
# 終端機 A
.\build\lab04_bridge.exe normal 14660 127.0.0.1 14650 60
# 終端機 B
.\build\lab01_telemetry.exe 14650 127.0.0.1 14660 5
```

退出碼要在該命令執行完立刻讀 `$LASTEXITCODE`。執行過其他原生命令後，它可能已經被覆蓋。

## Lab 01：Telemetry Reader

### 任務

從 UDP 讀取 HEARTBEAT 與 ATTITUDE，顯示來源身分與姿態。發送自己的 HEARTBEAT，找到對端後鎖定該飛控；失聯時要能退出並回報原因。

自己實作時，先拆成三步：完成 socket 與有限等待；完成 bytes → message；最後才做 discovery 與資料輸出。別在第一個函式同時處理所有事情。

### 執行

```powershell
# A
.\build\lab04_bridge.exe normal
# B
.\build\lab01_telemetry.exe
```

成功時會有 `HEARTBEAT sys=1 comp=1 armed=0`、`ATTITUDE deg ...` 與 `SUMMARY attitudes=...`。姿態數值與筆數隨排程而變，不應逐字比對整份 log。

斷線實驗：把 bridge 的時間縮短成一秒，client 跑五秒：

```powershell
# A 啟動後，立即在 B 執行
.\build\lab04_bridge.exe normal 14560 127.0.0.1 14550 1
# B
.\build\lab01_telemetry.exe 14550 127.0.0.1 14560 5
```

手動切換終端機太慢可能錯過 discovery；自動測試使用啟動握手避免這件事。預期已 discovery 的 client 在最後 heartbeat 約兩秒後輸出 `HEARTBEAT_TIMEOUT`，退出碼 3。若一開始完全沒有對端，輸出 `DISCOVERY_TIMEOUT`，退出碼 2。

### 解答怎麼讀

先讀 [Lab 01 main.cpp](../labs/lab01_telemetry/src/main.cpp)，再讀 [lab.hpp](../common/include/lab.hpp) 的 `Udp`、`Parser`、`Client::poll()`。

`observer` 只處理已經被 Client 選定來源的完整訊息。姿態輸出不影響 discovery 與 heartbeat 計時。這樣將來加電池或位置訊息時，就不用重寫 transport。

自我檢查：拔掉 ATTITUDE 但保留 HEARTBEAT，會不會觸發 HEARTBEAT_TIMEOUT？答案是不會；兩種資料的新鮮度應分開追蹤。此 Lab 結束時若完全沒收到 ATTITUDE，會退出碼 4，但尚未實作持續姿態 watchdog。

## Lab 02：Command Client

### 任務

送出 Arm / Disarm，等待匹配的 ACK；沒有 ACK 就有界限地重送。ACK ACCEPTED 後，還要等新的 HEARTBEAT 確認 armed 狀態。

### 正常流程

```powershell
# A
.\build\lab04_bridge.exe normal 14560 127.0.0.1 14550 60
# B，依序執行
.\build\lab02_command.exe arm
.\build\lab02_command.exe disarm
```

預期 Arm 出現 `ACK ACCEPTED` 與 `STATE_CONFIRMED armed=1`，Disarm 則為 `armed=0`。bridge 中會印出 `VEHICLE_COMMAND id=400 confirmation=0`。

### 故障注入與應觀察的差異

每個案例重啟 bridge，把 mode 換成下表名稱，再執行 `lab02_command.exe arm`。

| bridge mode | 模擬情境 | client 預期 | 退出碼 |
| --- | --- | --- | --- |
| `normal` | 接受並改變狀態 | ACCEPTED、STATE_CONFIRMED | 0 |
| `deny` | Commander 拒絕 Arm | DENIED，result=2 | 3 |
| `drop-first` | 執行第一筆但丟掉第一次 ACK | attempt=2，之後確認狀態 | 0 |
| `silent-ack` | 有執行、但所有 ACK 都不傳 | 三次發送後 COMMAND_TIMEOUT | 3 |
| `no-state` | 故意回 ACCEPTED，卻不改變狀態 | STATE_CONFIRM_TIMEOUT | 4 |
| `progress` | 先回進度，800 ms 後回最終成功 | IN_PROGRESS、ACCEPTED，不出現 attempt=2 | 0 |
| `spoof` | 只送錯來源、錯收件者或錯命令的 ACK | 忽略錯誤 ACK，最後 timeout | 3 |

`spoof` 是匹配邏輯的測試名稱，沒有實作密碼學偽造或身分認證。`progress` 只模擬成功的非同步回覆，不是完整長任務排程器。

### 解答怎麼讀

[Lab 02 main.cpp](../labs/lab02_command/src/main.cpp) 只決定 Arm / Disarm 參數與目標狀態。真正交易流程在 `Client::command()`；ACK 篩選在 `ack_matches()`；ACK 後的觀察在 `confirm()`。

練習題：`silent-ack` 後飛控到底有沒有 armed？本範例會有，因為 bridge 在丟掉 ACK 前已經執行命令；但 client 單靠命令 timeout 無法確定。這是為什麼 timeout log 使用 outcome unknown。

延伸練習：做 `MAV_CMD_SET_MESSAGE_INTERVAL` 的 client 以要求 ATTITUDE 速率。這需要對端實作該命令；現有 bridge 未支援，會回 UNSUPPORTED，不能直接把失敗判定為 client 寫錯。

## Lab 04：MAVLink Bridge 與 Commander

這個 Lab 先於 Lab 03 閱讀，因為它提供教學用飛控對端。

### 任務

把外部 COMMAND_LONG 轉成內部命令，讓 Commander 決定結果，再回傳 COMMAND_ACK；定期把內部狀態編碼成 HEARTBEAT 與遙測。Commander 不能依賴 socket API。

### 解答導讀

讀 [Lab 04 main.cpp](../labs/lab04_bridge/src/main.cpp) 的 receive callback，順著資料看：`decode` → `target` 檢查 → `VehicleCommand` → `Commander::command()` → `ack()`。接著看主迴圈如何定期從 Commander 讀取 armed/offboard/position 並輸出。

再讀 [commander.hpp](../labs/lab04_bridge/include/commander.hpp)。它沒有任何 IP/port；你可以用假時間直接測試，而不必啟動網路。`labs/lab00_protocol/src/main.cpp` 就用這個方式測預送、冪等、失聯與座標。

### 接回既有 PX4-Lite 的作業

1. 對照你原本 command topic 與 `VehicleCommand` 的欄位，補來源身分與回覆 metadata。
2. 用 publish 替代直接呼叫；Commander 訂閱後產生 ack topic。
3. 讓 bridge 訂閱 ack 與姿態 topic，編碼成 MAVLink。
4. 用 Lab 01 / 02 作為外部端，保留兩邊 log。

目前提供的是獨立可執行參考版。尚未取得原有 PX4-Lite 專案，所以沒有假稱已完成 topic 移植。

## Lab 03：Offboard Controller

### 任務

持續提供位置與 yaw setpoint，切入 Offboard、Arm、移動至目標，觀察位置收斂，再停止控制流測試模擬端的失聯策略。

### 執行

```powershell
# A：重新啟動，避免沿用上一個練習的狀態
.\build\lab04_bridge.exe normal
# B：預設移動觀察六秒
.\build\lab03_offboard.exe
```

預期看見三次命令流程：切模式、Arm、最後 Disarm；中間還有：

```text
POSITION_REACHED=1 x=... z=...
STOP_SETPOINTS (client heartbeat continues)
OFFBOARD_LOSS_CONFIRMED
```

bridge 會印 `OFFBOARD_LOSS -> HOLD (toy policy)`。正常退出碼 0。位置誤差門檻是 x 與 z 各 0.1 m，且位置訊息收到時間距今不足 0.5 秒。本版未計算整段軌跡 RMS 誤差，也沒有真實飛行控制器。

### 解答怎麼讀

先看 [Lab 03 main.cpp](../labs/lab03_offboard/src/main.cpp) 的 `stream` callback，再看所有 `command()` 與 `confirm()` 呼叫如何把它傳進去。預送 1.3 秒，之後在命令等待期間仍持續送 20 Hz 控制資料。

進入 Offboard 使用 DO_SET_MODE，param1 為 CUSTOM_MODE_ENABLED、param2 為 PX4 main mode 6。教學 bridge 刻意採用這個慣例，並以 `6 << 16` 回報 custom mode，方便後續對照 PX4；不能因此宣稱 generic autopilot 都使用 PX4 的 mode 編碼。

停止 setpoint 後仍會 `poll()`，所以 client HEARTBEAT 繼續。這讓測試能針對控制流中斷。程式會拒絕 `MAV_AUTOPILOT_GENERIC` 以外的飛控，因為這個練習的結尾刻意製造控制流失聯；真正 SITL 的移植需求見手冊第 7 章。

### 練習題與答案

題目：把 z 改成 +2，模型會往哪邊走？答案：LOCAL_NED 下方。先在模擬模型看數值，不要直接用錯誤座標去飛真機。

題目：為何不能 `sleep(2s)` 等 ACK？答案：這會讓同一執行緒停止 setpoint 發送；本版選擇有限等待的 poll 與服務 callback。

題目：client 中斷後，bridge 會自動 Disarm 嗎？答案：不會。toy failsafe 只退出 Offboard、停止位置跟隨，armed 狀態保留；正常範例最後另送 Disarm。請分開觀察 mode 與 armed。

## 自動驗證

```powershell
ctest --test-dir build --output-on-failure -V
```

`protocol` 執行 C++ 檢查，包含 CRC、分包、多包、ACK 匹配、mask、Commander 與假時間失聯。`udp_integration` 由 Python 啟動真正的 C++ client / bridge 行程，以 OS 分配的本機 UDP 埠跑十個情境。

Python 只負責行程管理與驗收，不替代 MAVLink 傳輸或編碼。測試依據退出碼與關鍵 log 判斷，沒有把時間變動的浮點輸出寫死。

若 CMake 沒找到 Python，整合測試不會註冊，不能因為只看到一項 CTest 通過就說全部完成。可明確設定：

```powershell
cmake -S . -B build -DPython3_EXECUTABLE="C:/path/to/python.exe"
ctest --test-dir build -N
```

正常應列出 `protocol` 與 `udp_integration` 兩項。原始測試輸出、環境與未驗證項目請看 [驗證報告](驗證報告.md)。
