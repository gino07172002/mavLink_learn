# MAVLink 重複練習入口

每個 Lab 都把程式、操作說明、練習、解答與原版基準放在同一個資料夾。建議順序是 **00 → 04（先啟動）→ 01 → 02 → 04（細讀）→ 03 → 05 → 06 → 07 → 08 → 09**。

| Lab | 學什麼 | 主要執行目標 |
| --- | --- | --- |
| [00 封包與協定](lab00_protocol/README.md) | parser、CRC、半包、多包、來源隔離 | `protocol_tests` |
| [01 遙測與 QGC](lab01_telemetry/README.md) | HEARTBEAT、ATTITUDE、解碼、單位與 timeout | `lab01_telemetry` |
| [02 命令](lab02_command/README.md) | COMMAND_LONG、ACK、重送、狀態確認 | `lab02_command` |
| [03 最小模型 Offboard](lab03_offboard/README.md) | NED、mask、預送、控制流失聯 | `lab03_offboard` |
| [04 Bridge](lab04_bridge/README.md) | pack、bytes、UDP、Commander 與合成姿態 | `lab04_bridge` |
| [05 真正 PX4 SIH](lab05_sitl/README.md) | 真飛控遙測、飛行、落地與失聯 | `lab05_sitl` |
| [06 原有 PX4-Lite](lab06_px4lite/README.md) | 原 Commander／控制器／物理模型橋接 | `lab06_px4lite`（可選） |
| [07 進階服務](lab07_services/README.md) | Parameter、TIMESYNC、Mission | `lab07_services` |
| [08 Router](lab08_router/README.md) | 單載具、雙 client 的固定轉送 | `lab08_router` |
| [09 Signing](lab09_signing/README.md) | key、重播、篡改、unsigned policy | `signing_tests` |

## 每個資料夾怎麼用？

```text
labNN_topic/
├── README.md          ← 前置、命令、預期結果、程式導讀
├── 練習.md            ← 先預測再動手，每輪填自己的紀錄
├── 解答.md            ← 練習解說、常見誤判
├── CMakeLists.txt     ← 由專案根目錄納入建立
├── src/main.cpp      ← 真正編譯的程式，可以修改練習
├── include/          ← 只有本 Lab 需要的 header（部分 Lab）
├── tests/            ← 本 Lab 的自動驗證（部分 Lab）
├── docs/             ← QGC 詳細教學與紀錄（Lab 01）
└── reference/        ← 已驗證原版快照；重設工具的來源
```

`reference/` 不參與正常編譯。不要把答案改了，卻期待重設工具仍能找回原版；工具會用 SHA-256 檢查基準，發現被改動就拒絕還原。

## 固定的練習節奏

1. 選一個 Lab，讀其 README，使用乾淨的對端狀態。
2. 看練習題，先寫「我預測畫面／log 會變成什麼」。
3. 改 `src/` 或指定的本 Lab 檔案，重新編譯、重新啟動。
4. 記錄數值、退出碼與解釋，再比對解答。
5. 還原並再次驗收，再做下一輪。

所有 CMake／執行命令都從專案根目錄操作；不用 `cd` 到各 Lab 才建立。Windows 執行檔仍在 `build/`，Linux 在 `build-linux/`，原有名稱不變。各 Lab 的 CMakeLists 是根專案的子專案，不是獨立 configure 入口。

## 還原與備份

```powershell
python scripts/reset_lab.py --list
python scripts/reset_lab.py 01 --dry-run
python scripts/reset_lab.py 01
```

重設前先把目前已列管的檔案備份到 `practice_backups/<UTC時間>/lab01_telemetry/`。新增檔案、練習紀錄、其他 Lab 與外部專案不會被刪除。備份程式不自動清理舊紀錄。

本章練習若改到別的章，例如 Lab 01 修改的是發送端 pitch，實際改的是 Lab 04，應還原 `04`。共用 helper 要另用 `common` 還原。

## 共用與跨 Lab 的部分

- [common/](../common/README.md)：一份 Parser、UDP、Client，避免各章有不同實作。
- [tests/integration.py](../tests/integration.py)：Lab 01～04 的聯合情境，不屬於單一 Lab。
- [scripts/start-qgc-demo.ps1](../scripts/start-qgc-demo.ps1)：啟動 QGC 與教學 bridge。
- [完整手冊](../docs/教學手冊.md)、[後續實作](../docs/PX4_SITL與後續實作.md)：跨章的背景與導讀。
- [驗證紀錄](../docs/後續驗證報告.md)：歷史證據保留原樣，當時 log 內的舊來源路徑不代表目前目錄。

## 每輪進度

| Lab | 第一輪：能重現 | 第二輪：能修改並預測 | 第三輪：能不看答案解釋 |
| --- | --- | --- | --- |
| 00 | [ ] | [ ] | [ ] |
| 01 | [ ] | [ ] | [ ] |
| 02 | [ ] | [ ] | [ ] |
| 03 | [ ] | [ ] | [ ] |
| 04 | [ ] | [ ] | [ ] |
| 05 | [ ] | [ ] | [ ] |
| 06 | [ ] | [ ] | [ ] |
| 07 | [ ] | [ ] | [ ] |
| 08 | [ ] | [ ] | [ ] |
| 09 | [ ] | [ ] | [ ] |
