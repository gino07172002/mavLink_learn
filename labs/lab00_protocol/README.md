# Lab 00 — 封包與協定檢查

用可執行檢查理解 bytes 如何變成有效 MAVLink frame。重點是 parser 保留狀態、CRC 損壞拒收，以及不同來源不能共用半包狀態。

## 前置與適用對端

會使用 CMake 與 C++ 的基本資料型別即可。第一次只讀 parser 檢查；Commander／Offboard 檢查可在 Lab 03、04 後回來看。

## 開始前

從 [專案首頁](../../README.md) 執行一次 CMake configure。以下所有命令的工作目錄都是 **D:\mavLink_learn 專案根目錄**，不是這個 Lab 資料夾。只需建立本章 target；執行檔名稱與原先保持一致。

Windows 要改寫執行中的 EXE 前，先在該程式的前景終端按 Ctrl+C。若由背景腳本啟動，先確認並停止那個 bridge PID，再編譯；不要關掉其他不相關程式。

## 執行與驗收

```powershell
cmake --build build --parallel 1 --target protocol_tests
.\build\protocol_tests.exe
ctest --test-dir build -R "^protocol$" --output-on-failure -V
```

基準程式輸出 CHECKS=26。它同時包含封包、ACK 與玩具 Commander 的檢查，不代表真 PX4 已啟動。

## 程式導讀

先讀 src/main.cpp 的分包、多包與壞 CRC 段，再讀共用 Parser::feed()。注意 decode callback 只會接到完整有效的訊息。

- [目前可修改的程式](src/main.cpp)：CMake 實際建立的版本。
- [原版參考解答](reference/src/main.cpp)：整理當下已驗證的基準，保留作為比對。
- [練習與紀錄表](練習.md)：每次先預測，再改程式、執行與記錄。
- [解答與講解](解答.md)：做完再比對；答案不取代實際觀察。
- [共用通訊基礎](../../common/README.md)：Parser、UDP、Client，供各 Lab 使用。

## 還原後再練一次

```powershell
python scripts/reset_lab.py 00 --dry-run
python scripts/reset_lab.py 00
```

重設前先備份目前檔案到專案的 `practice_backups/`，再還原本 Lab 的程式、header、測試與 CMake 基準。自己新增的檔案與筆記不會刪除；共用程式、其他 Lab 與外部 PX4-Lite 都不會被一起還原。完成後重新編譯受影響的 target。

[回到全部 Lab](../README.md) · [完整手冊](../../docs/教學手冊.md) · [原始驗證證據](../../docs/後續驗證報告.md)
