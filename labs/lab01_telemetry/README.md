# Lab 01 — 遙測與 QGroundControl

先親眼看見 HEARTBEAT 與姿態，再追蹤 C++ 接收端。先建立「程式改了什麼，地面站就看見什麼」的對應。

## 前置與適用對端

先完成 Lab 00 的封包基本概念。這個 Lab 的對端使用 Lab 04 bridge。

## 開始前

從 [專案首頁](../../README.md) 執行一次 CMake configure。以下所有命令的工作目錄都是 **D:\mavLink_learn 專案根目錄**，不是這個 Lab 資料夾。只需建立本章 target；執行檔名稱與原先保持一致。

Windows 要改寫執行中的 EXE 前，先在該程式的前景終端按 Ctrl+C。若由背景腳本啟動，先確認並停止那個 bridge PID，再編譯；不要關掉其他不相關程式。

## 執行與驗收

```powershell
cmake --build build --parallel 1 --target lab01_telemetry lab04_bridge
# 終端機 A：獨立測試埠，不占用 QGC 的 14550
.\build\lab04_bridge.exe normal 14660 127.0.0.1 14650 600
# 終端機 B
.\build\lab01_telemetry.exe 14650 127.0.0.1 14660 6
```

看到 sys=1、comp=1、ATTITUDE deg 與 SUMMARY attitudes=正數。QGC 另依本 Lab 的 docs 操作，不要讓 QGC 與 Lab 01 同時 bind 14550。

本 Lab 的 QGC 操作：[接入教學](docs/QGroundControl接入教學.md)、[實際串接紀錄](docs/QGroundControl串接.md)。

## 程式導讀

從 src/main.cpp 的 observer 看 HEARTBEAT 與 ATTITUDE decode，再追共用 Client::poll() → Udp::receive() → Parser::feed()。

- [目前可修改的程式](src/main.cpp)：CMake 實際建立的版本。
- [原版參考解答](reference/src/main.cpp)：整理當下已驗證的基準，保留作為比對。
- [練習與紀錄表](練習.md)：每次先預測，再改程式、執行與記錄。
- [解答與講解](解答.md)：做完再比對；答案不取代實際觀察。
- [共用通訊基礎](../../common/README.md)：Parser、UDP、Client，供各 Lab 使用。

## 還原後再練一次

```powershell
python scripts/reset_lab.py 01 --dry-run
python scripts/reset_lab.py 01
```

重設前先備份目前檔案到專案的 `practice_backups/`，再還原本 Lab 的程式、header、測試與 CMake 基準。自己新增的檔案與筆記不會刪除；共用程式、其他 Lab 與外部 PX4-Lite 都不會被一起還原。完成後重新編譯受影響的 target。

[回到全部 Lab](../README.md) · [完整手冊](../../docs/教學手冊.md) · [原始驗證證據](../../docs/後續驗證報告.md)
