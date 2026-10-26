# MAVLink generated headers

來源：[mavlink/c_library_v2](https://github.com/mavlink/c_library_v2)

本次實測固定 commit：`6b4243f0e54b42ddb1e65df7a3a5e8c99d15ec1f`。

`mavlink/` 是固定於該 commit 的 Git submodule，程式使用官方 generated `common/mavlink.h`；這不是自己寫的 MAVLink 編碼器。包含 common 所引用的 standard、minimal 定義，亦保留上游其他 dialect。上游內容未修改。

若 clone 時沒有取得這個目錄，於專案根目錄執行：

```powershell
git submodule update --init --recursive
git -C third_party/mavlink rev-parse HEAD
```

本機最初下載使用 shallow clone。若要切換較舊版本且歷史不足，可先 `git -C third_party/mavlink fetch --unshallow`。僅在更新教材並重新驗證時才更新依賴，並提交父版本庫的 gitlink 變更；不要為了重新編譯就追蹤最新 main。不要在已有 submodule 的目錄裡再次執行 `git submodule add`。

MAVLink 上游授權背景見 [MAVLink 官方專案](https://github.com/mavlink/mavlink#license)。實際散布時請一併檢查所用產生碼與依賴的授權；本教材不另行改寫上游授權。
