# HttpOTA

ESP32 上的 HTTP OTA 服务 + 新固件"验证期"回滚：设备上开一个 HTTP 口，
一条 GET 就能让它去下载并烧写新固件；烧上去的固件如果反复重启起不来，
自动回滚到上一个稳定版本，不至于把设备刷成砖。

依赖都用 arduino-esp32 内核自带的 —— `WebServer` / `HTTPUpdate` / `Preferences`，
不需要往 `lib_deps` 里加东西。

by: ZiUtility

---

## 一、使用方法

### 1. 目录结构

```
lib/HttpOTA/
├── httpota.h / httpota.cpp   ← 库本体：设备端的 OTA 服务 + 验证期回滚
├── partitions.csv            ← 分区表：双 app + otadata（见第 3 节）
├── ota_upload.py             ← PC 端：把 bin 摆出去并触发升级，pio run -t upload 调它
├── update_version.py         ← PC 端：每次编译把版本号流水号 +1
├── version_template.h        ← 版本号模板：工程里没有 version.h 时由脚本复制过去
└── README.md
```

整个目录拷到别的工程就能连构建工具一起带走：三个工具放哪儿都行（第 3 节说明
为什么），而 `include/version.h` 必须落在工程的 `include/` 下 —— 它是被编译进
固件的头文件，由脚本自动生成，不随库分发。

### 2. 接入步骤

```cpp
#include "httpota.h"

static OtaAssist s_ota;      // 端口默认 80

void setup()
{
    ExtLogger::instance().init(...);   // 日志先起来 —— 回滚过程那几条要靠它发出去
    s_ota.setVersion(SW_VERSION);      // ① 登记版本号，会打进启动第一条日志

    s_ota.stableCheck();               // ② 必须放在 setup() 最前面（理由见设计逻辑）

    wifi_link_begin();                 // 先把 STA 网络接口拉起来
    s_ota.initService();               // ③ 起 HTTP 服务（不等连上，关联后即达）
}

void loop()
{
    s_ota.loop();                      // ④ 每轮调用，受理 HTTP 请求

    // ⑤ 跑够时间就确认这个版本稳定：不确认的话，OTA 上去的固件重启满
    //    3 次会被判为不稳定并自动回滚（判据由应用决定，见设计逻辑）
    if (!confirmed && millis() >= 30000) { confirmed = true; s_ota.confirm(); }
}
```

- ① 是可选的：不登记就显示 `unknown`。版本号由应用提供，因为它是工程的概念
  （本工程在 `include/version.h` 的 `SW_VERSION`，编译时自增）。
- 启动第一条日志长这样，OTA 之后靠它确认新固件进来了、从哪个分区起的：

  ```
  14:32:07 I [pump_display.OTA]: version = 1.0.035, partition = OTA_1, status = OTA_PENDING
  ```

工程里的实际接法见 [`src/main.cpp`](../../src/main.cpp)（OTA 小节）。

### 3. 构建工具与 platformio.ini

platformio.ini（本工程的实际写法，工具都在库目录里）：

```ini
; 自定义OTA更新
; OTA 这套构建工具（分区表、上传脚本、版本号脚本）都放在库里，
; 整个 lib/HttpOTA/ 目录拷到别的工程就能一起带走
upload_protocol = custom
custom_target_ip = 192.168.0.165 ; 注意每次新设备务必对应修改IP，不然刷错设备
board_build.partitions = lib/HttpOTA/partitions.csv
upload_command = python lib/HttpOTA/ota_upload.py $SOURCE ${this.custom_target_ip}
extra_scripts = pre:lib/HttpOTA/update_version.py
```

**需要注意 `include/version.h`**，原因有两条：

- 工程源码里是 `#include "version.h"`（靠 `-Iinclude` 找到），它必须待在工程的
  `include/` 下；
- 它是 `update_version.py` 生成并自增的，缺文件的情形都能自己处理：
  文件**不存在**就从 `version_template.h` 复制一份过去；文件**在但没有
  `SW_VERSION` 行**就在末尾补一行，原有内容不动。所以新工程**不用手建**这个文件。

版本号格式 `1.0.001`：最后一段是流水号，每次编译 +1；若最后一段不是数字
（比如手改成 `1.2.beta`），脚本会把那行注释掉、以同样的前缀从 `001` 重新开始
（下一版是 `1.2.001`）。

**`version_template.h` 是"起始版本"**：里面的版本号就是新工程第一次编译时的版本
（复制过去之后本次**不再自增**，所以是模板里写的 `1.0.001`，不是 `1.0.002`）。
想让自己以后的工程从别的版本起算，改模板里那一行即可。模板万一也不在，脚本会
退回内置的默认内容并打一条告警。

**拷到别的工程时要改的四处**：

- `custom_target_ip` —— 改成设备 IP；
- `ota_upload.py` 的 `PORT = 8000` —— 和别的东西撞端口时改；
- 两个脚本里的 `include/version.h` —— 版本头文件不叫这名或不放这时要改；
- `partitions.csv` —— 按自己芯片的 flash 大小调整（见下）。

`partitions.csv` 至少要有这几行：

```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     ,       0x5000,
otadata,  data, ota,     ,       0x2000,
app0,     app,  ota_0,   ,       0x300000,
app1,     app,  ota_1,   ,       0x300000,
```

- **两个 `app` 分区**：升级时新固件写进空闲的那个，正在跑的那个原封不动 ——
  写坏了还能回滚回来。只有一个 app 分区时 `httpUpdate` 直接报错。
- **`otadata`**：记"下次从哪个分区启动"。没有它 `esp_ota_set_boot_partition()`
  不生效，回滚也就无从谈起。
- 两个 app 分区各自都要放得下固件（本工程 3MB，当前固件约 1.3MB）。

**`lib_deps` 不用加任何东西** —— `WebServer` / `HTTPUpdate` / `Preferences`
都是 arduino-esp32 内核自带的。

**串口烧录固件时**：如果上一次 OTA 留下了"待验证"状态，新烧的固件会被当成
那个不稳定的版本继续数重启次数。烧录前先调一次 `clearOtaData()` 清掉，
烧完再注释掉 —— 这个开关是手动的，库没法自己判断"这次是不是串口烧的"。

### 4. HTTP 接口

| 请求                        | 行为                                                                                                  |
| --------------------------- | ----------------------------------------------------------------------------------------------------- |
| `GET /`                     | 探活 + 报当前固件：`Device OK. version = 1.0.035, partition = OTA_1, status = OTA_PENDING`            |
| `GET /update?url=<固件URL>` | 去下载该 URL 的 `.bin` 并烧写，**成功后设备直接重启**；失败返回 500 + 失败原因，缺 `url` 参数返回 400 |

`/update` 的结果以**设备端**为准：成功了设备会重启（连接被掐断是正常的），
失败会回 500 并把原因写进日志，比如：

```
14:32:07 E [pump_display.OTA]: update failed (-1): HTTP error: connection refused
```

本工程的 `pio run -t upload` 走的就是这条路（见 [`ota_upload.py`](ota_upload.py)）：

```
PC：在本机 8000 端口起一个 HTTP 服务，把 build 出来的 firmware.bin 摆出去
 → GET http://<设备IP>/update?url=http://<PC_IP>:8000/firmware.bin
 → 设备下载、烧写、重启
```

烧写过程中设备会在串口打一条进度条（`[====>    ] 42%  520 kB / 1.2 MB`）。

### 5. 版本状态机

状态记在 NVS 的 `ota` 命名空间里，重启后接着算：

| 状态          | 值  | 含义                       | 怎么进入                                          |
| ------------- | --- | -------------------------- | ------------------------------------------------- |
| `UART_STABLE` | 5   | 串口烧录的版本，天然算稳定 | NVS 里没有记录（首次上电、或刚 `clearOtaData()`） |
| `OTA_NEW`     | 1   | 刚 OTA 完成，待验证        | OTA 写完固件时由库写入                            |
| `OTA_PENDING` | 2   | 验证中                     | 启动时读到 `OTA_NEW`                              |
| `OTA_STABLE`  | 3   | 验证通过，成为新的回滚基线 | 应用调用 `confirm()`                              |
| `OTA_INVALID` | 4   | 判定不稳定                 | `PENDING` 状态下重启满 3 次仍未 `confirm()`       |

NVS 里的三个键：

| 键               | 类型 | 含义                                    |
| ---------------- | ---- | --------------------------------------- |
| `APP_STATUS`     | int  | 上表的状态                              |
| `VERIFY_TIMES`   | int  | `PENDING` 期间的重启次数，到 3 就判失败 |
| `STABLE_SUBTYPE` | int  | 上一个稳定版本所在分区，回滚的目标      |

### 6. 注意事项

- **`stableCheck()` 必须放最前面**：判定失败时它会 `delay(5s)` 然后 `esp_restart()`，
  放在后面等于白跑一遍初始化再重启。
- **必须有人调 `confirm()`**：OTA 上去的固件如果一直不确认，重启 3 次就回滚。
- **PC 端脚本打印"上传完成"≠升级成功**：它只负责把 bin 摆出去、发一条指令，
  升级成没成要看设备端（重启了 / 日志里的 `update failed` / `curl http://<设备>/`
  看到的版本号）。所以 `/update` 的返回值在库里是要看的，见设计逻辑。
- **下载期间主循环被阻塞**：界面停住不动，MQTT 也会因为 `loop()` 没在跑而掉线，
  升级完自动重连 —— 属于预期行为，不是卡死。
- **进度条只走串口**，不进日志（理由见设计逻辑）。
- **没有鉴权**：同一局域网内谁都能给 `/update` 发指令。只适合内网，别做端口映射。

---

## 二、库的设计逻辑

### 为什么要"验证期"

OTA 最坏的情况不是升级失败 —— 升级失败设备还跑着老固件，是安全的。
真正危险的是**新固件烧进去了但起不来**：串口烧录时还能插上线救回来，OTA 场景下
设备可能装在够不着的地方。所以需要一道自动回滚：新固件先"试用"，试用期内
反复重启起不来，就自己换回上一个能用的版本。

### 为什么记在 NVS

"这个固件好不好"这个判断跨重启，必须落盘。三个键各管一件事：
当前状态、重启次数、回滚目标。用 `Preferences`（NVS 的 Arduino 封装）而不是
文件系统，是因为这里只有三个整数、一次几十微秒，不该为它挂一个文件系统。

### 为什么用"重启次数"而不是"运行时长"

因为要防的那种固件**根本跑不到计时器**。用运行时长判定的话，一个启动就崩的
固件永远攒不够时长、也就永远判不出失败，只能一直崩下去；而重启次数是每次
启动都会 +1 的，崩三次就够判定了。

### 回滚到哪儿

优先回 `STABLE_SUBTYPE` 记着的那个分区 —— 上一次被 `confirm()` 确认过的版本。
没有记录（比如从没 OTA 过）就退而求其次，用 `esp_ota_get_next_update_partition()`
拿另一个 app 分区。所以分区表必须是**双 app**（`ota_0` + `ota_1` + `otadata`），
单 app 的表连升级都做不了。

### `confirm()` 的判据为什么交给应用

"固件算不算正常"是业务问题，库答不了：对一个传感器节点是"采到数据了"，
对这块屏可能是"界面起来了、MQTT 通了"。所以库只提供 `confirm()` 这个动作，
什么时候调由应用定。本工程取的是**连续运行满 30 秒**：够覆盖 WiFi + MQTT + SNTP
的启动过程，又不至于让"能跑但马上会崩"的固件蒙混过去。

刻意**不**用"MQTT 通了"当判据 —— broker 挂掉或路由器抽风时，那会把本来好好的
新固件判成不稳定，白白回滚一次。

### 进度条为什么不走日志

进度回调每秒触发几十次，走 `XLOG` 会往 UDP 日志里灌几十行/秒，把真正有用的
日志冲掉，还把网络占满 —— 而升级恰恰是最需要链路干净的时候。进度条本来就是
给串口终端看的（人和设备挨着才需要看进度），所以这里直接用 `Serial.print`
是有意的例外，不是漏改。

### 为什么一定要看 `update()` 的返回值

因为**失败和成功在设备这端差别巨大，在 PC 那端却一模一样**：

- 成功：`update()` 内部写完分区就 `esp_restart()`，它后面那行代码根本不会执行；
- 失败：`update()` 返回一个错误码，设备继续跑**老固件** —— 界面照常、网络照常、
  MQTT 照常，唯一的区别是"这次升级没有发生"。

PC 端的 `ota_upload.py` 只知道"bin 摆出去了、指令也发出去了"，两种情况它都打印
"上传完成"。所以不看返回值的话，失败是完全静默的，现象只是"升级完一看还是老版本"，
一点线索都没有。现在失败会打一条 E 级日志、回 500，把原因字符串（连不上、404、
固件不匹配……）直接给出来。

### 版本号为什么也交给应用登记

和 `confirm()` 是同一个道理：库自己取不到"工程意义上的版本号"。IDF 的 app
description 里那个 version 字段，在 Arduino/PlatformIO 构建下默认没有意义
（是编译系统给的，不是工程定义的）；本工程真正的版本号在 `include/version.h`
的 `SW_VERSION` 里，由 `update_version.py` 每次编译自增。所以做成 `setVersion()`
由应用登记，库只负责把它打进日志。

### 已知问题

- `/update` 没有鉴权，也没有校验固件签名，见"注意事项"最后一条。
