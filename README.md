# 用 C++ 學 MAVLink：從封包到 PX4 整合

這份教材依照原始 [guide.txt](guide.txt) 擴寫。目標是讓你能自己寫出 UDP MAVLink client，並把已經學過的 Pub/Sub、Commander 與外部通訊接起來。

**從 [Lab 練習入口](labs/README.md) 開始。** 程式與文件已按 Lab 00～09 分組，每章都有 README、可修改程式、練習題、解答與原版快照。先預測、修改、觀察，再還原重做。

背景概念見 [教學手冊](docs/教學手冊.md)，初階操作總覽見 [Lab 操作與解答](docs/Labs.md)。共用通訊基礎在 [common/](common/README.md)，驗證紀錄在 [驗證報告](docs/驗證報告.md)。

現階段接入地面站請先讀 [QGroundControl 接入教學](labs/lab01_telemetry/docs/QGroundControl接入教學.md)，可用 `scripts/start-qgc-demo.ps1` 重現；另有 [QGroundControl 5.1.2 實際串接紀錄](labs/lab01_telemetry/docs/QGroundControl串接.md)。

後續從 [真正 PX4 SITL、原有 PX4-Lite 與進階實作](docs/PX4_SITL與後續實作.md) 繼續，新增 Lab 05～08；結果見 [後續驗證報告](docs/後續驗證報告.md)。

## 先建立與執行

所有下方指令都在專案根目錄執行。各 Lab 的 `CMakeLists.txt` 由根專案納入，執行檔仍輸出到 `build/`／`build-linux/`，原有名稱不變。

第一次下載時一併取得固定版本的 MAVLink submodule：

```powershell
git clone --recurse-submodules https://github.com/gino07172002/mavLink_learn.git
cd mavLink_learn
```

若先前只做一般 clone，再執行 `git submodule update --init --recursive`。下載 GitHub ZIP 不包含 submodule；建議使用 Git clone。

Windows PowerShell，工作目錄為本專案根目錄。需要 Git、CMake 3.20+、支援 C++17 的編譯器；自動測試與重設工具使用 Python 3.9+。此工作區已附官方 generated C headers，通常不需重新下載。

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 1
ctest --test-dir build --output-on-failure -V
```

`--parallel 1` 限制編譯時的記憶體用量。沒有 Ninja 時，MinGW 可以改用 `-G "MinGW Makefiles"`；改 generator 要使用另一個 build 目錄。MSVC 請在其開發者命令列建立，執行檔通常在 `build/Debug/`，CTest 要加 `-C Debug`。

開兩個 PowerShell 終端機：

```powershell
# 終端機 A：教學用飛控端，執行 30 秒
.\build\lab04_bridge.exe normal
```

```powershell
# 終端機 B：接收六秒遙測
.\build\lab01_telemetry.exe
# 接著試 Arm / Disarm，或重新啟動 bridge 後試 Offboard
.\build\lab02_command.exe arm
.\build\lab02_command.exe disarm
.\build\lab03_offboard.exe
```

Linux 原始碼有 POSIX socket 分支，建立指令如下；此環境的實測範圍請見驗證報告。

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux --parallel 1
ctest --test-dir build-linux --output-on-failure -V
./build-linux/lab04_bridge normal
# 另一個終端機：./build-linux/lab01_telemetry
```

## 學習順序

完整章節清單與三輪練習紀錄在 [labs/README.md](labs/README.md)。目前對話的 pack／UDP／姿態修改說明已整理到 [Lab 04 解答](labs/lab04_bridge/解答.md)，接收端與下一步練習放在 [Lab 01 解答](labs/lab01_telemetry/解答.md)。

| 順序 | 閱讀／動手做 | 完成後要能解釋 |
| --- | --- | --- |
| 1 | 手冊第 1～3 章、Lab 01 | 位元組如何變成姿態資料？timeout 根據什麼判斷？ |
| 2 | 手冊第 4 章、Lab 02 | 為什麼 sendto 成功、ACK 成功、狀態改變是三件事？ |
| 3 | 手冊第 5 章、Lab 04 | 解碼與業務邏輯如何分離？如何接上自己的 Commander？ |
| 4 | 手冊第 6 章、Lab 03 | NED、mask、預送流、失聯保護如何一起運作？ |
| 5 | 手冊第 7 章、SITL 驗收表 | 如何用真正 PX4 對照這些簡化模型？ |
| 6 | 手冊第 8 章 | Parameter、Mission、Routing、Signing 各解決什麼問題？ |

Lab 編號延續原始路線，建議實作順序是 01 → 02 → 04 → 03，因為 04 提供其餘 Lab 可用的本機對端。

## 重複練習與還原

```powershell
python scripts/reset_lab.py --list
python scripts/reset_lab.py 04 --dry-run
python scripts/reset_lab.py 04
```

修改 `labs/labNN_topic/src/main.cpp`，保留 `reference/` 原版不動。重設會先備份現有檔案到 `practice_backups/`，再還原所選 Lab，自己新增的檔案與筆記不刪除。改過共用 helper 時，用 `common` 另行還原。還原後仍需重新編譯、重啟。

目錄整理後已重新建立並驗證，結果見 [目錄整理與重設工具驗證](docs/目錄整理驗證.md)。

## 範圍

Lab 04 bridge 是自行撰寫的小型 Commander 與運動模型。後續 Lab 06 另外直接編譯相鄰 `px4-lite_learning` 專案的原有模組；Lab 05 則使用既有 WSL 的真正 PX4 SIH。三種對端的實作與驗收分開記錄。

Lab 03 刻意停止 setpoint，限定 sysid 1 的 generic 教學端；真正 PX4 正常飛行與失聯實驗使用 Lab 05，原有 PX4-Lite 使用 `--lite`／`--lite-loss`。原教材 v1.17 尚未執行，本次真正 PX4 實測版本是 `6db88017b8`（v1.18 開發版）。

原始路線保留於 `guide.txt`。第三方版本來源與重建方式見 [third_party/README.md](third_party/README.md)。

## 檢查教材與 CI

```powershell
python scripts/check_materials.py
```

這會檢查本機文件連結與原版快照 SHA，不要求你的練習程式與原版相同。`.gitattributes` 固定 LF，避免不同平台 checkout 後因換行不同而破壞原版 SHA。

GitHub Actions 在 Windows／Linux 建立並執行不依賴外部 PX4-Lite 的五組基本測試。Lab 06、真正 PX4 SIH、QGC GUI 仍需對應本機環境，歷史實測結果見驗證文件。此版本庫不包含 QGC、PX4 binary 或外部 PX4-Lite 專案。
