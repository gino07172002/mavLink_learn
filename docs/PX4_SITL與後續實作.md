# 後續實作：真正 PX4、原有 PX4-Lite 與進階協定

這一章接在 [QGroundControl 接入教學](../labs/lab01_telemetry/docs/QGroundControl接入教學.md) 之後。前面的 Lab 01～04 保留作為最小練習；這裡另外加入 Lab 05～08，避免把玩具模型的驗收與真正 PX4 混在一起。

## 1. 三個對端必須分清楚

| 對端 | 實際程式 | 代表什麼 |
| --- | --- | --- |
| 最小教學飛控 | `lab04_bridge` | 小型同步 Commander、一階位置模型，方便故障注入 |
| 原有 PX4-Lite | `lab06_px4lite` | 直接編譯你原有的 Commander、控制器、Mixer 與剛體／馬達模型 |
| 真正 PX4 | 原有 WSL `PX4-Autopilot/.../bin/px4` | PX4 模組、uORB、EKF、控制器、SIH 與真實 MAVLink 模組 |

這台電腦既有原始碼在 `D:\px4-lite_learning`；真正 PX4 在 Ubuntu 的 `/home/gino/PX4-Autopilot`。先前只有檢查目前工作區，未找到這些相鄰專案；這次已查明並使用。

實測 PX4 binary 透過 `ver all` 回報 hash **`6db88017b8120d6f97fa84541d5765ef99ef7392`**，原始碼 describe 是 `v1.18.0-beta1-360-g6db88017b8`。它與前面閱讀路線引用的 v1.17.0 不同，不能把這次結果標成 v1.17 通過。

## 2. Lab 05：讓 C++ 控制真正 PX4 SIH

### 為什麼先用 SIH？

SIH 是 PX4 內建模擬路徑，可在無頭環境驗證飛控程式。這次不用 3D 視窗，讓問題集中在連線、命令、估測、控制與狀態觀察。這是軟體模擬，沒有真實硬體或真實空域飛行。

WSL 初次啟動曾有 RPC 失敗；本次成功啟動後，仍出現 `Failed to start the systemd user session` 警告，但原生 Linux C++ 與 PX4 執行成功。為避免重複等待 user session，實測透過持續開啟的 WSL shell 操作。不需要為此重設發行版或刪除既有環境。

### 建立 Linux 版 client

在 Ubuntu shell：

```bash
cd /mnt/d/mavLink_learn
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux --parallel 1
ctest --test-dir build-linux --output-on-failure
```

本次使用 GCC 11.4.0、Python 3.10.12。Linux socket 分支已實際編譯、執行，不再只是預留的原始碼。

### 一個命令跑完真飛控驗證

```bash
python3 labs/lab05_sitl/tests/sitl_runner.py --scenario normal
```

若 PX4 binary 不在預設路徑：

```bash
python3 labs/lab05_sitl/tests/sitl_runner.py --scenario normal \
  --px4-binary /your/PX4-Autopilot/build/px4_sitl_default/bin/px4
```

runner 會建立專用暫存工作目錄、啟動 SIH Quad-X（airframe 10040）、使用 PX4 instance 7，並建立專用 UDP instance：PX4 本地 14660，C++ client 本地 14650，雙方都在 WSL 的 127.0.0.1。

這個測試不依賴 Windows QGC；QGC 教學 bridge 仍使用 Windows 的 14560／14550。runner 在啟動後停止自己這個 PX4 行程的預設 MAVLink instances，再開專用 instance；不會去關其他 PX4 行程。

每次參數儲存在該次暫存 rootfs，結束後移除，不修改既有 PX4 專案的參數檔。原生 stdout、C++ 輸出、JSON 結果保存在 `build-linux/sitl-evidence/<scenario>/`；runner 也會保留該次產生的 ULog（如果有）。

### 正常流程實際驗什麼？

1. Lab 01 讀取真 PX4 HEARTBEAT 與 ATTITUDE。
2. Lab 02 Arm，再等新的 HEARTBEAT armed 位元；接著 Disarm 並確認。
3. Lab 05 要求 LOCAL_POSITION_NED 與 EXTENDED_SYS_STATE 串流。
4. 確認資料新鮮、載具在地面且未解鎖。
5. 20 Hz 預送 setpoint 兩秒，切 Offboard，解鎖。
6. 相對起點升高 2 m、向北移動 2 m、返回起點上方。
7. 切 AUTO.LAND，等實際 landed state，再確認 Disarm。

每個位置階段必須三維誤差小於 0.3 m、速度小於 0.4 m/s，並連續滿足一秒。等待 ACK 與 HEARTBEAT 期間持續發 setpoint；每次控制迴圈也檢查位置 freshness、heartbeat、模式與操作範圍。

成功輸出有 `REACHED TAKEOFF`、`REACHED NORTH_2M`、`REACHED RETURN_HOME`、`LAND_MODE_CONFIRMED`、`LANDED_CONFIRMED`、`DISARM_CONFIRMED`，最後是 `SITL_PASS scenario=normal-flight`。

最後一輪實際位置例子：向北階段觀察到 x 約 1.92 m、z 約 -1.97 m；返回階段 x 約 0.20 m、z 約 -1.99 m。這是瞬間樣本與門檻驗收，不是完整追蹤誤差或控制品質的統計報告。

### 獨立的失聯實驗

```bash
python3 labs/lab05_sitl/tests/sitl_runner.py --scenario loss
```

另一個新的 PX4 instance runtime 先升高 2 m，之後**只停止 Setpoint，繼續送 client HEARTBEAT**。本實驗明確設定並在 PX4 log 印出：

| 參數 | 值 | 用意 |
| --- | --- | --- |
| COM_RC_IN_MODE | 4 | 此無實體遙控器模擬設定使用 stick input disabled |
| COM_RCL_EXCEPT | 4 | 此版本的 Offboard RC-loss exception bit |
| COM_OF_LOSS_T | 0.5 s | 控制流失聯逾時設定 |
| COM_OBL_RC_ACT | 4 | 此版本設定為 Land；以實際模式與落地結果確認 |

程式必須看到離開 Offboard，而且主模式 AUTO=4、子模式 LAND=6，才算觀察到預期 failsafe。實測 custom mode 為 `100925440`（`4 << 16 | 6 << 24`），接著出現落地、Disarm 與 `SITL_PASS scenario=offboard-loss`。

本程式沒有強制解鎖、沒有停用估測器或感測器 preflight 檢查。出現飛行錯誤時會嘗試切 LAND 並等待落地；若復原無法確認，輸出 `RECOVERY_UNCONFIRMED`，不會假稱安全落地。此程式要求明確 `--sitl`／`--sitl-loss`，限定 loopback；這些限制不是硬體／模擬的密碼學證明，僅應連到 runner 啟動的模擬行程。

### 解答導讀

讀 `labs/lab05_sitl/src/main.cpp`：先找 `fresh_position`、`fresh_landed`，再找 `stream`、`reach`、`land`、`wait_landed`。接著看 `labs/lab05_sitl/tests/sitl_runner.py` 如何管理行程、參數、命令與驗收輸出。

跟 Lab 03 最大的學習差異是：**真飛控需要以實際位置、速度、模式、landed state 與 armed state 收尾**，不能送完命令就當作完成。

## 3. Lab 06：接回原本的 PX4-Lite

### 直接重用哪些程式？

| 原有檔案 | 新 bridge 如何使用 |
| --- | --- |
| `lab07/commander.cpp`、`commander.hpp` | 編譯原有 Commander，執行 Arm／Disarm、Takeoff、Land 與健康失效轉換 |
| `simulator/src/simple_quad_simulator.cpp` | 編譯原有 250 Hz 剛體、馬達與地面接觸模型 |
| `lab06/control_stack.hpp` | 使用原有姿態 P、角速度 PID |
| `supplementary/02_position_control/position_control.hpp` | 使用原有位置／速度串級控制 |
| `supplementary/04_control_allocator/control_allocator.hpp` | 使用原有馬達分配器 |

原有專案檔案沒有修改；由目前工作區的 CMake 以 `PX4LITE_ROOT` 直接編譯／包含。pitch 在 adapter 邊界做與原本飛行 demo 相同的符號處理，避免改動原控制器與模型。

```powershell
# Windows，專案根目錄
cmake -S . -B build -DPX4LITE_ROOT=D:/px4-lite_learning
cmake --build build --parallel 1
ctest --test-dir build -R px4lite_udp_integration --output-on-failure -V
```

```bash
# Ubuntu
cmake -S . -B build-linux -DPX4LITE_ROOT=/mnt/d/px4-lite_learning
cmake --build build-linux --parallel 1
ctest --test-dir build-linux -R px4lite_udp_integration --output-on-failure -V
```

這個路徑是可選依賴。沒設定時，一般教學 Lab 仍可建立；分享專案給別人時，要同時提供相容的 PX4-Lite checkout，不能把 `lab06_px4lite` 當成完全獨立的執行目標。

### 資料流與 topic 邊界

```text
UDP → decode → 命令 FIFO（包含來源、命令與參數）
                 ↓
           原有 Commander
                 ↓
        ACK FIFO → MAVLink COMMAND_ACK

位置目標 → 原有控制器 → Mixer → 原有 Simulator
                                  ↓
                        最新狀態 topic（含 generation）
                                  ↓
                         MAVLink 遙測 sender
```

原先 Pub/Sub 練習的類別嵌在各自 `main.cpp`，不是可直接引用的共用 library。因此新 adapter 使用單執行緒命令／ACK FIFO 與最新狀態 topic；它沒有宣稱直接連結那個既有 Topic 類別，也不是真正 uORB。命令使用 FIFO，是因為連續兩筆命令不能像最新姿態一樣任意覆寫。

body/controller/plant 依固定 4 ms tick 執行；MAVLink 遙測另有頻率。這讓通訊與控制用途不必綁在同一個 rate。程式對落後 tick 設 catch-up 上限，還沒有完成硬即時排程或完整 jitter 量測。

### 手動操作與驗收

```powershell
# A：原有模組的 MAVLink adapter，sysid=61
.\build\lab06_px4lite.exe 14660 127.0.0.1 14650 120
# B：正常飛行
.\build\lab05_sitl.exe --lite 14650 127.0.0.1 14660 25
```

失聯實驗要重啟 adapter，再用 `--lite-loss`。這兩個選項使用與真 PX4 相同的外部控制流程，但檢查 generic autopilot、sysid 61，輸出是 **PX4LITE_PASS**，不標成真 PX4 SITL 成功。

Windows 與 Linux 都已實際通過正常飛行與失聯降落。失聯時 adapter 透過原 Commander 的健康輸入觸發 FAILSAFE → LANDING，再以原有位置控制器下降；這個故障訊號映射是教學 adapter 的設計，並不等同 PX4 的健康／失聯模組實作。

目前健康輸入預設 true、位置使用模擬器真值，不是感測器與 EKF 融合。yaw 只支援零值；支援的 frame／mask 與操作範圍也有限制。這些限制保留在程式與教學，避免把橋接成功誤認為完整自主飛控。

## 4. Lab 07：Parameters、TIMESYNC 與 Mission

這個 client 只用在 runner 建立的**未解鎖、SIH 10040、初始任務為空**的私有模擬端。它不會執行上傳的任務。

```bash
python3 labs/lab05_sitl/tests/sitl_runner.py --scenario services
```

解答在 `labs/lab07_services/src/main.cpp`。三個服務都直接使用 MAVLink messages；不要把它們全部包成 COMMAND_LONG。

### Parameters 的重點

讀取 `SYS_AUTOSTART` 時，先檢查型別是 INT32，再以 `memcpy` 還原 PX4 bytewise 編碼；不能把 wire float 直接數值轉型。16 字元的 param ID 也可能沒有結尾零，讀取時要限制長度。

接著讀 `COM_OF_LOSS_T` 的 REAL32 值，寫成 0.8、重新請求讀回驗證，再恢復原值。失敗路徑也會嘗試恢復；runner 的暫存參數目錄則提供最後的隔離。正式產品還需完整參數列表、缺項重傳、快取與 metadata，這個 Lab 是單參數交易練習。參考 [Parameter Protocol](https://mavlink.io/en/services/parameter.html)。

### TIMESYNC 的重點

發送 `tc1=0`、`ts1=本機 monotonic ns`，匹配回覆中鏡射的 ts1；tc1 是對端時間。量測 RTT，使用本地發送／接收中點估計 offset，取得八筆樣本後輸出中位數。

這是估計，不是把本機與 PX4 的 UTC 校時，也不是保證延遲對稱。不同 clock 的 offset 很大仍可能正常。正式產品應拒絕高 RTT 樣本並維持濾波器；協定見 [Time Synchronization](https://mavlink.io/en/services/timesync.html)。

### Mission 的重點

client 先下載 count，拒絕覆蓋非空任務。接著宣告三項任務，依 PX4 的 MISSION_REQUEST_INT 逐項回覆 Takeoff、Waypoint、Land，等待 MISSION_ACK；再逐項下載核對 command、座標與高度，最後 CLEAR_ALL，確認 count 回到零。

這個練習不切 Mission mode、不解鎖。任務座標是 SIH 練習用固定座標，不能拿去執行真實任務。讀回與清除都需要完整握手；完成 download 後要回 MISSION_ACK，不能只收到最後一項就不管。參考 [Mission Protocol](https://mavlink.io/en/services/mission.html)。

## 5. Lab 08：兩個 client 共用一個對端

`labs/lab08_router/src/main.cpp` 提供固定星狀拓撲：一條 flight link、兩條 client link。每條鏈路有獨立 parser。飛控訊息 fan-out 到兩端；任何 client 訊息只往飛控，不往另一個 client 再轉，避免這個拓撲形成回圈。

例如停止原來直接送 QGC 的教學 bridge，改成：

```powershell
# A：bridge 改送 router 的 flight bind
.\build\lab04_bridge.exe normal 14560 127.0.0.1 14670 600
# B：flight bind/peer、QGC bind/peer、C++ bind/peer、秒數
.\build\lab08_router.exe 14670 14560 14671 14550 14672 14650 600
# C：C++ client，QGC 同時維持 UDP 14550 AutoConnect
.\build\lab02_command.exe arm 14650 127.0.0.1 14672
```

QGC 使用其自身的 system ID，C++ client 使用 245，正式多 client 應維持唯一身分。這個 router 尚未做動態路由表、多飛控目的地查找、重複包抑制或簽章政策，範例只用未簽章 loopback；不稱為完整 MAVLink router。

自動測試 `router_integration` 使用兩個獨立 C++ client：其中一端讀遙測，另一端 Arm；驗證後者取得 ACK，且前者也從新 HEARTBEAT 看見 armed=1。這比只確認兩邊「有封包」更能驗證雙向資料流。

## 6. Signing：先驗證政策，再談部署

`labs/lab09_signing/src/main.cpp` 使用官方 generated library 做六項檢查：輸出簽章旗標、正確金鑰接受、重播拒絕、簽章被改動拒絕、錯金鑰拒絕、嚴格政策拒絕未簽章封包。

```powershell
ctest --test-dir build -R signing --output-on-failure -V
```

測試金鑰是公開、固定的**測試資料**，不是部署用秘密。這裡沒有替 PX4 或 QGC 配置真實簽章鏈路，也沒有安全儲存、金鑰輪換或跨重啟時間持久化；簽章只驗證完整性與來源相關資訊，不加密 payload。參考 [Message Signing](https://mavlink.io/en/guide/message_signing.html)。

## 7. 配合實際版本讀原始碼

本次直接檢查了 WSL 原始碼，以下行號對應 hash `6db88017b8120d6f97fa84541d5765ef99ef7392`；若切換版本請重新搜尋函式名稱。

| 檔案／入口 | 本次找到的行號 | 觀察重點 |
| --- | --- | --- |
| `mavlink_receiver.cpp` → `handle_message_command_long` | 494 | 參數與來源如何成為內部命令 |
| 同檔 → `_cmd_pub.publish` | 715 等 | 命令發布點與模組內處理分支 |
| 同檔 → `handle_message_set_position_target_local_ned` | 1177 | target、frame、mask 與 setpoint |
| 同檔 → `_offboard_control_mode_pub.publish` | 1274 | 模式生命訊號 topic |
| 同檔 → `_trajectory_setpoint_pub.publish` | 1282 | 軌跡目標發布條件 |
| `commander/Commander.cpp` → Arm command 分支 | 1089 | 接受條件、狀態變化與回覆 |
| `mavlink_main.cpp` → `vehicle_command_ack` | 2956 起 | 內部結果如何成為外部 ACK |
| `streams/ATTITUDE.hpp` → `send()` | 62 | topic 讀取、姿態轉換與編碼 |

把 runner 的 `ver all`、`commander status`、`listener vehicle_local_position`、`listener vehicle_land_detected` 輸出與這些函式對照，才能把外部觀察接回飛控內部流程。

## 8. 仍然不代表什麼？

以上練習完成通訊與模擬驗證，不代表已完成真機飛行、Gazebo 3D／視覺感測器驗證、長時間 soak、硬即時排程、全套 Mission／Parameter 功能或正式金鑰管理。QGC 接到最小 bridge 的 Not Ready／缺 metadata 也仍是已知限制；不能把新增的真 PX4 測試結果套到那個 bridge 上。

最新通過項目與原始證據見 [後續驗證報告](後續驗證報告.md)。
