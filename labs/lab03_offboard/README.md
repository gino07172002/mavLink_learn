# Lab 03 — 最小模型 Offboard

把一次性命令與持續 setpoint 串流放進同一事件迴圈，練習 NED、type_mask、預送與控制流失聯。

## 前置與適用對端

先完成 Lab 01、02，並讀 Lab 04。此 Lab 僅用 sysid 1 的 generic 玩具 bridge；真正 PX4／PX4-Lite 使用 Lab 05 的選項。

## 開始前

從 [專案首頁](../../README.md) 執行一次 CMake configure。以下所有命令的工作目錄都是 **D:\mavLink_learn 專案根目錄**，不是這個 Lab 資料夾。只需建立本章 target；執行檔名稱與原先保持一致。

Windows 要改寫執行中的 EXE 前，先在該程式的前景終端按 Ctrl+C。若由背景腳本啟動，先確認並停止那個 bridge PID，再編譯；不要關掉其他不相關程式。

## 執行與驗收

```powershell
cmake --build build --parallel 1 --target lab03_offboard lab04_bridge
# A：重新啟動模型
.\build\lab04_bridge.exe normal 14660 127.0.0.1 14650 600
# B
.\build\lab03_offboard.exe 14650 127.0.0.1 14660 6
```

POSITION_REACHED=1、STOP_SETPOINTS、OFFBOARD_LOSS_CONFIRMED，最後 Disarm；正常退出碼 0。這裡的 loss policy 只是保留最後位置數值。

## 程式導讀

先找 stream callback，再看 command() 和 confirm() 如何在等待時持續呼叫它。最後看停止串流後仍維持 poll()。

- [目前可修改的程式](src/main.cpp)：CMake 實際建立的版本。
- [原版參考解答](reference/src/main.cpp)：整理當下已驗證的基準，保留作為比對。
- [練習與紀錄表](練習.md)：每次先預測，再改程式、執行與記錄。
- [解答與講解](解答.md)：做完再比對；答案不取代實際觀察。
- [共用通訊基礎](../../common/README.md)：Parser、UDP、Client，供各 Lab 使用。

## 還原後再練一次

```powershell
python scripts/reset_lab.py 03 --dry-run
python scripts/reset_lab.py 03
```

重設前先備份目前檔案到專案的 `practice_backups/`，再還原本 Lab 的程式、header、測試與 CMake 基準。自己新增的檔案與筆記不會刪除；共用程式、其他 Lab 與外部 PX4-Lite 都不會被一起還原。完成後重新編譯受影響的 target。

[回到全部 Lab](../README.md) · [完整手冊](../../docs/教學手冊.md) · [原始驗證證據](../../docs/後續驗證報告.md)
