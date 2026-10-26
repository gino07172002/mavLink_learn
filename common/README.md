# 共用 MAVLink 通訊基礎

[include/lab.hpp](include/lab.hpp) 是各 Lab 使用的同一份 Parser、UDP 與 Client。CMake 的 `lab_support` 讓每個 Lab 可以直接 `#include "lab.hpp"`。Lab 04 的玩具 Commander 與 Lab 06 的 adapter header 則放回各自章節。

| 層次 | 入口 | 工作 |
| --- | --- | --- |
| 網路 | `Udp::receive()` / `send_bytes()` | bind、select、recvfrom、sendto、固定來源過濾 |
| 封包 | `Parser::feed()` / `encode()` | 使用官方 generated library 解析與序列化 |
| 連線觀察 | `Client::poll()` / `discover()` | heartbeat、來源選擇與資料 cache |
| 命令交易 | `command()` / `ack_matches()` | 匹配 ACK、有限重送、IN_PROGRESS |
| 狀態觀察 | `confirm()` | 等新的 heartbeat 與指定狀態 |

收取姿態的路徑是：

```text
recvfrom → bytes → Parser::feed → 完整 message
        → Client 來源篩選 → Lab 01 observer → ATTITUDE decode → 顯示
```

發送端的 `encode()` 呼叫 `mavlink_msg_to_send_buffer()` 產生 bytes，再由 `sendto()` 決定目的 IP 與 port。封包 header 的 system/component ID 表示發送者；不能把它們當成網路地址。

共用程式有獨立的 `reference/` 基準。真的要修改時，先記下會影響哪些 Lab，停止需重新建立的執行檔，再建置並跑回歸測試。

```powershell
python scripts/reset_lab.py common --dry-run
python scripts/reset_lab.py common
cmake --build build --parallel 1
ctest --test-dir build --output-on-failure
```

重設一個 Lab 不會自動還原此檔，也不會還原其他 Lab。
