# Lab 08 — 雙 client 訊息轉送

一端操作命令，另一端觀察相同飛控遙測。每個 UDP peer 有獨立 parser，資料只在既定 star topology 內轉送。

## 前置與適用對端

先完成 Lab 01、02、04。固定單載具、兩個 client、loopback、未簽章拓撲。

## 開始前

從 [專案首頁](../../README.md) 執行一次 CMake configure。以下所有命令的工作目錄都是 **D:\mavLink_learn 專案根目錄**，不是這個 Lab 資料夾。只需建立本章 target；執行檔名稱與原先保持一致。

Windows 要改寫執行中的 EXE 前，先在該程式的前景終端按 Ctrl+C。若由背景腳本啟動，先確認並停止那個 bridge PID，再編譯；不要關掉其他不相關程式。

## 執行與驗收

```powershell
cmake --build build --parallel 1 --target lab08_router lab04_bridge lab01_telemetry lab02_command
ctest --test-dir build -R router_integration --output-on-failure -V
# 手動埠配置見下方「跨章教學」連結的 Lab 08
```

ROUTER_PASS，命令 client 收到 armed=1，而獨立遙測 client 也看見 armed=1。手動串接 QGC 的完整埠配置見 [跨章教學](../../docs/PX4_SITL與後續實作.md) 的 Lab 08。

## 程式導讀

src/main.cpp 的 flight.receive、gcs.receive、client.receive；tests/integration.py 以六個獨立臨時埠驗證。

- [目前可修改的程式](src/main.cpp)：CMake 實際建立的版本。
- [原版參考解答](reference/src/main.cpp)：整理當下已驗證的基準，保留作為比對。
- [練習與紀錄表](練習.md)：每次先預測，再改程式、執行與記錄。
- [解答與講解](解答.md)：做完再比對；答案不取代實際觀察。
- [共用通訊基礎](../../common/README.md)：Parser、UDP、Client，供各 Lab 使用。

## 還原後再練一次

```powershell
python scripts/reset_lab.py 08 --dry-run
python scripts/reset_lab.py 08
```

重設前先備份目前檔案到專案的 `practice_backups/`，再還原本 Lab 的程式、header、測試與 CMake 基準。自己新增的檔案與筆記不會刪除；共用程式、其他 Lab 與外部 PX4-Lite 都不會被一起還原。完成後重新編譯受影響的 target。

[回到全部 Lab](../README.md) · [完整手冊](../../docs/教學手冊.md) · [原始驗證證據](../../docs/後續驗證報告.md)
