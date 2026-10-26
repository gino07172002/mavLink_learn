# Lab 09 — 簽章與重播防護

使用官方 generated library 觀察簽章與接收政策：正確 key、錯 key、重播、竄改與 unsigned 封包。

## 前置與適用對端

先完成 Lab 00，理解 CRC 與身分認證不是同一件事。此 Lab 是 library 級測試，沒有配置真 PX4/QGC 簽章鏈路。

## 開始前

從 [專案首頁](../../README.md) 執行一次 CMake configure。以下所有命令的工作目錄都是 **D:\mavLink_learn 專案根目錄**，不是這個 Lab 資料夾。只需建立本章 target；執行檔名稱與原先保持一致。

Windows 要改寫執行中的 EXE 前，先在該程式的前景終端按 Ctrl+C。若由背景腳本啟動，先確認並停止那個 bridge PID，再編譯；不要關掉其他不相關程式。

## 執行與驗收

```powershell
cmake --build build --parallel 1 --target signing_tests
.\build\signing_tests.exe
ctest --test-dir build -R "^signing$" --output-on-failure -V
```

六項 PASS，最後 SIGNING_CHECKS=6。固定測試 key 是公開資料，不能拿來部署。

## 程式導讀

src/main.cpp 的 tx signing state、receiver signing streams、feed() 與六個 require()。先看有效包，再看 replay 與 policy。

- [目前可修改的程式](src/main.cpp)：CMake 實際建立的版本。
- [原版參考解答](reference/src/main.cpp)：整理當下已驗證的基準，保留作為比對。
- [練習與紀錄表](練習.md)：每次先預測，再改程式、執行與記錄。
- [解答與講解](解答.md)：做完再比對；答案不取代實際觀察。
- [共用通訊基礎](../../common/README.md)：Parser、UDP、Client，供各 Lab 使用。

## 還原後再練一次

```powershell
python scripts/reset_lab.py 09 --dry-run
python scripts/reset_lab.py 09
```

重設前先備份目前檔案到專案的 `practice_backups/`，再還原本 Lab 的程式、header、測試與 CMake 基準。自己新增的檔案與筆記不會刪除；共用程式、其他 Lab 與外部 PX4-Lite 都不會被一起還原。完成後重新編譯受影響的 target。

[回到全部 Lab](../README.md) · [完整手冊](../../docs/教學手冊.md) · [原始驗證證據](../../docs/後續驗證報告.md)
