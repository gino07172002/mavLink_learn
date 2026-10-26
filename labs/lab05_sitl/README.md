# Lab 05 — 真正 PX4 SIH

用相同底層 C++ client 操作真正 PX4，確認遙測、Arm、Offboard、位置收斂、AUTO.LAND 與 landed state。正常飛行和失聯實驗分開。

## 前置與適用對端

先完成 Lab 01～04。需要已可執行的 Linux／WSL PX4 SIH binary。先確認使用模擬端與獨立參數目錄。

## 開始前

從 [專案首頁](../../README.md) 執行一次 CMake configure。以下所有命令的工作目錄都是 **D:\mavLink_learn 專案根目錄**，不是這個 Lab 資料夾。只需建立本章 target；執行檔名稱與原先保持一致。

Windows 要改寫執行中的 EXE 前，先在該程式的前景終端按 Ctrl+C。若由背景腳本啟動，先確認並停止那個 bridge PID，再編譯；不要關掉其他不相關程式。

## 執行與驗收

```bash
# 以下在 Ubuntu shell、專案根目錄執行
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Debug
cmake --build build-linux --parallel 1
python3 labs/lab05_sitl/tests/sitl_runner.py --scenario normal
python3 labs/lab05_sitl/tests/sitl_runner.py --scenario loss
```

normal 與 loss 都有 SITL_RUNNER_PASS；client 最後是 SITL_PASS、LANDED_CONFIRMED、DISARM_CONFIRMED。每次使用新的暫存 rootfs。

## 程式導讀

src/main.cpp 的 freshness、stream、reach、land、wait_landed；tests/sitl_runner.py 負責啟動自己的 PX4、設定獨立埠與保存結果。

- [目前可修改的程式](src/main.cpp)：CMake 實際建立的版本。
- [原版參考解答](reference/src/main.cpp)：整理當下已驗證的基準，保留作為比對。
- [練習與紀錄表](練習.md)：每次先預測，再改程式、執行與記錄。
- [解答與講解](解答.md)：做完再比對；答案不取代實際觀察。
- [共用通訊基礎](../../common/README.md)：Parser、UDP、Client，供各 Lab 使用。

## 還原後再練一次

```powershell
python scripts/reset_lab.py 05 --dry-run
python scripts/reset_lab.py 05
```

重設前先備份目前檔案到專案的 `practice_backups/`，再還原本 Lab 的程式、header、測試與 CMake 基準。自己新增的檔案與筆記不會刪除；共用程式、其他 Lab 與外部 PX4-Lite 都不會被一起還原。完成後重新編譯受影響的 target。

[回到全部 Lab](../README.md) · [完整手冊](../../docs/教學手冊.md) · [原始驗證證據](../../docs/後續驗證報告.md)
