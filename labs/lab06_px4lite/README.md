# Lab 06 — 原有 PX4-Lite 整合

真正重用你原有 Commander、位置／姿態／角速度控制器、Mixer 與 SimpleQuadSimulator，透過新的單執行緒命令 FIFO／狀態 topic 接到 MAVLink。

## 前置與適用對端

需要相鄰 D:/px4-lite_learning；先了解 Lab 04 的 bridge 和 Lab 05 的觀察式控制流程。

## 開始前

從 [專案首頁](../../README.md) 執行一次 CMake configure。以下所有命令的工作目錄都是 **D:\mavLink_learn 專案根目錄**，不是這個 Lab 資料夾。只需建立本章 target；執行檔名稱與原先保持一致。

Windows 要改寫執行中的 EXE 前，先在該程式的前景終端按 Ctrl+C。若由背景腳本啟動，先確認並停止那個 bridge PID，再編譯；不要關掉其他不相關程式。

## 執行與驗收

```powershell
cmake -S . -B build -DPX4LITE_ROOT=D:/px4-lite_learning
cmake --build build --parallel 1 --target lab06_px4lite lab05_sitl
ctest --test-dir build -R px4lite_udp_integration --output-on-failure -V
# 手動：A 啟動 adapter，B 跑控制程式
.\build\lab06_px4lite.exe 14660 127.0.0.1 14650 120
.\build\lab05_sitl.exe --lite 14650 127.0.0.1 14660 25
```

sysid=61；正常與 --lite-loss 各用新 adapter，最後輸出 PX4LITE_PASS。它不是 PX4 binary，不輸出 SITL_PASS。

## 程式導讀

src/main.cpp 的命令／ACK queues、state topic 與固定 tick；include/lite_backend.hpp 連到原專案模組。

- [目前可修改的程式](src/main.cpp)：CMake 實際建立的版本。
- [原版參考解答](reference/src/main.cpp)：整理當下已驗證的基準，保留作為比對。
- [練習與紀錄表](練習.md)：每次先預測，再改程式、執行與記錄。
- [解答與講解](解答.md)：做完再比對；答案不取代實際觀察。
- [共用通訊基礎](../../common/README.md)：Parser、UDP、Client，供各 Lab 使用。

## 還原後再練一次

```powershell
python scripts/reset_lab.py 06 --dry-run
python scripts/reset_lab.py 06
```

重設前先備份目前檔案到專案的 `practice_backups/`，再還原本 Lab 的程式、header、測試與 CMake 基準。自己新增的檔案與筆記不會刪除；共用程式、其他 Lab 與外部 PX4-Lite 都不會被一起還原。完成後重新編譯受影響的 target。

[回到全部 Lab](../README.md) · [完整手冊](../../docs/教學手冊.md) · [原始驗證證據](../../docs/後續驗證報告.md)
