# Lab 04 — MAVLink Bridge 與 Commander

飛控對端把外部 MAVLink 命令交給小型 Commander，再回 ACK 與遙測。本 Lab 也收錄對話裡的 pack()、UDP 與姿態修改講解。

## 前置與適用對端

具備 Lab 00 基本概念即可先啟動，配合 Lab 01、02、03 使用。

## 開始前

從 [專案首頁](../../README.md) 執行一次 CMake configure。以下所有命令的工作目錄都是 **D:\mavLink_learn 專案根目錄**，不是這個 Lab 資料夾。只需建立本章 target；執行檔名稱與原先保持一致。

Windows 要改寫執行中的 EXE 前，先在該程式的前景終端按 Ctrl+C。若由背景腳本啟動，先確認並停止那個 bridge PID，再編譯；不要關掉其他不相關程式。

## 執行與驗收

```powershell
cmake --build build --parallel 1 --target lab04_bridge
# 使用明確埠與十分鐘期限
.\build\lab04_bridge.exe normal 14660 127.0.0.1 14650 600
```

立即印 BRIDGE_READY port=14660 mode=normal。沒有 client 時會持續發送；對端 absent 由通訊／應用期限處理，不代表有真正 UDP connection。

## 程式導讀

src/main.cpp 的 decode → target 檢查 → Commander::command → ACK，以及定期 HEARTBEAT／ATTITUDE；業務狀態在 include/commander.hpp。

- [目前可修改的程式](src/main.cpp)：CMake 實際建立的版本。
- [原版參考解答](reference/src/main.cpp)：整理當下已驗證的基準，保留作為比對。
- [練習與紀錄表](練習.md)：每次先預測，再改程式、執行與記錄。
- [解答與講解](解答.md)：做完再比對；答案不取代實際觀察。
- [共用通訊基礎](../../common/README.md)：Parser、UDP、Client，供各 Lab 使用。

## 還原後再練一次

```powershell
python scripts/reset_lab.py 04 --dry-run
python scripts/reset_lab.py 04
```

重設前先備份目前檔案到專案的 `practice_backups/`，再還原本 Lab 的程式、header、測試與 CMake 基準。自己新增的檔案與筆記不會刪除；共用程式、其他 Lab 與外部 PX4-Lite 都不會被一起還原。完成後重新編譯受影響的 target。

[回到全部 Lab](../README.md) · [完整手冊](../../docs/教學手冊.md) · [原始驗證證據](../../docs/後續驗證報告.md)
