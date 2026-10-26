# Lab 07 — 參數、TIMESYNC 與 Mission

把 Parameter、TIMESYNC 與 Mission 視為各自的交易協定，練習回讀、恢復、任務握手與時間域，而不是通通塞進 COMMAND_LONG。

## 前置與適用對端

先讀 Lab 05。只對 runner 啟動、未解鎖、airframe=10040、初始任務為空的私有 PX4 SIH 執行；不執行上傳的任務。

## 開始前

從 [專案首頁](../../README.md) 執行一次 CMake configure。以下所有命令的工作目錄都是 **D:\mavLink_learn 專案根目錄**，不是這個 Lab 資料夾。只需建立本章 target；執行檔名稱與原先保持一致。

Windows 要改寫執行中的 EXE 前，先在該程式的前景終端按 Ctrl+C。若由背景腳本啟動，先確認並停止那個 bridge PID，再編譯；不要關掉其他不相關程式。

## 執行與驗收

```bash
# Ubuntu shell，專案根目錄
cmake --build build-linux --parallel 1 --target lab07_services lab01_telemetry
python3 labs/lab05_sitl/tests/sitl_runner.py --scenario services
```

PARAM_INT SYS_AUTOSTART=10040、PARAM_WRITE_READ_RESTORE、TIMESYNC samples=8、MISSION_UPLOAD_DOWNLOAD_CLEAR、SERVICES_PASS。證據放在 build-linux/sitl-evidence/services/。

## 程式導讀

src/main.cpp 的 observer、wait、read_param/set_float、TIMESYNC 與 Mission 上傳／下載／清除；runner 共用 Lab 05 的 tests。

- [目前可修改的程式](src/main.cpp)：CMake 實際建立的版本。
- [原版參考解答](reference/src/main.cpp)：整理當下已驗證的基準，保留作為比對。
- [練習與紀錄表](練習.md)：每次先預測，再改程式、執行與記錄。
- [解答與講解](解答.md)：做完再比對；答案不取代實際觀察。
- [共用通訊基礎](../../common/README.md)：Parser、UDP、Client，供各 Lab 使用。

## 還原後再練一次

```powershell
python scripts/reset_lab.py 07 --dry-run
python scripts/reset_lab.py 07
```

重設前先備份目前檔案到專案的 `practice_backups/`，再還原本 Lab 的程式、header、測試與 CMake 基準。自己新增的檔案與筆記不會刪除；共用程式、其他 Lab 與外部 PX4-Lite 都不會被一起還原。完成後重新編譯受影響的 target。

[回到全部 Lab](../README.md) · [完整手冊](../../docs/教學手冊.md) · [原始驗證證據](../../docs/後續驗證報告.md)
