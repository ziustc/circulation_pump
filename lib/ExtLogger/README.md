# ExtLogger

Arduino / ESP32 上的轻量日志模块：printf 风格的日志宏、串口 + UDP 两个输出通道、
断网缓冲，以及可在运行时切换的日志等级。

除 Arduino 内核和（可选的）WiFi 外零依赖 —— 把整个 `ExtLogger/` 目录拷到工程的
`lib/` 下即可使用，不需要改 `platformio.ini`，也不需要联网拉依赖。

版本 v2.0.20261004 · by ZiUtility

---

## 一、使用方法

### 1. 目录结构

```
lib/ExtLogger/
├── extlogger.h     ← 公开接口 + 全部编译期配置
├── extlogger.cpp
└── README.md
```

PlatformIO 会把 `lib/` 下的每个目录当作私有库编译，拷进去就能 `#include "extlogger.h"`。

### 2. 三步接入

```cpp
#include "extlogger.h"

void setup()
{
    // ① 初始化：主机名用于在日志里区分设备
    ExtLogger::instance().init("pump_display", true); // true = 日志带真实时间

    // ② 打开需要的通道
    ExtLogger::instance().enableSerial(115200);
    ExtLogger::instance().enableUDP("192.168.0.10", 10001);
}

void loop()
{
    // ③ 周期调用：WiFi 重连后补发断网期间缓冲的日志
    ExtLogger::instance().loop();
}
```

- `init()` 的第二个参数为 `true` 时日志时间取真实时钟（需先同步 SNTP）。
  没同步上会自动退回 `millis()`，不会打出 1970 年的时间戳。
- `init()` 之前或干脆不调用也能写日志：主机名用 `EXTLOGGER_DEFAULT_HOSTNAME`
  （默认 `"ESP32"`），只是通道没开时不会有任何输出。

### 3. 编译期配置

全部用 `#ifndef` 包住，在 `platformio.ini` 里用 `build_flags` 覆盖，不必改源码：

```ini
build_flags =
    -DEXTLOGGER_ENABLE_UDP=0          ; 省 1KB RAM 和一段 Flash
    -DEXTLOGGER_BUFFER_SIZE=4096      ; 断网缓冲加大到 4KB
    -DEXTLOGGER_MIN_LEVEL=1           ; 正式版：把 XLOGD 整句从固件里去掉
```

| 宏                           | 默认值             | 作用                                                                                                                 |
| ---------------------------- | ------------------ | -------------------------------------------------------------------------------------------------------------------- |
| `EXTLOGGER_ENABLE_UDP`       | ESP 平台 1，其它 0 | UDP 通道。关掉后 `enableUDP()/disableUDP()` **不再声明**，调用会编译报错（比默默失效好），环形缓冲及相关代码一并消失 |
| `EXTLOGGER_ENABLE_LOCK`      | ESP32 为 1，其它 0 | FreeRTOS 互斥量，允许其它任务同时写日志                                                                              |
| `EXTLOGGER_BUFFER_SIZE`      | `1024`             | 环形缓冲字节数，仅 UDP 通道使用                                                                                      |
| `EXTLOGGER_MAX_LINE`         | `512`              | 单行上限，必须小于 `EXTLOGGER_BUFFER_SIZE`（有 `#error` 兜底）                                                       |
| `EXTLOGGER_HOSTNAME_MAX`     | `24`               | 主机名字段长度                                                                                                       |
| `EXTLOGGER_DEFAULT_HOSTNAME` | `"ESP32"`          | 没调 `init()` 时用的主机名                                                                                           |
| `EXTLOGGER_MIN_LEVEL`        | `0`（DEBUG）       | 编译期下限，低于它的日志宏整句不编进固件                                                                             |
| `EXTLOGGER_DEFAULT_LEVEL`    | `1`（INFO）        | 开机时的运行时等级                                                                                                   |

### 4. 写日志

| 宏                     | 等级       | 用途                       |
| ---------------------- | ---------- | -------------------------- |
| `XLOGD(tag, fmt, ...)` | DEBUG      | 调试细节，正式版通常关掉   |
| `XLOGI(tag, fmt, ...)` | INFO       | 正常流程的关键节点         |
| `XLOGW(tag, fmt, ...)` | WARN       | 不对但还没坏（重试、降级） |
| `XLOGE(tag, fmt, ...)` | ERROR      | 已经坏了                   |
| `XLOG(tag, fmt, ...)`  | 同 `XLOGI` | v1 的旧名字，保留兼容      |

```cpp
XLOG("MQTT", "connect failed, rc=%d.", client.state());
XLOGW("NET", "SNTP retry #%d", n);
```

日志行格式：

```
14:32:07 I [pump_display.MQTT]: connect failed, rc=-2.
└─时间──┘ │ └───主机名───┘└标签┘  └────正文────┘
         └ 等级：D / I / W / E
```

时间在“未开真实时间”或“还没同步上”时是 `millis()`，此时不是 8 个字符宽。
正文里调用方自己加的 `\r` `\n` 会被去掉（整行统一以 `\r\n` 收尾）。

### 5. 日志等级

等级数值越大越严重：`DEBUG < INFO < WARN < ERROR < NONE`。低于当前等级的日志
直接丢弃，且**不产生格式化开销**。开机等级是 `EXTLOGGER_DEFAULT_LEVEL`（默认 INFO）。

```cpp
ExtLogger::instance().setLevel(ExtLogger::Level::DEBUG);
ExtLogger::instance().setLevelByName("warn");   // 名字（不分大小写）
ExtLogger::instance().level();                  // 当前等级
```

`setLevelByName()` 认：`debug` `info` `warn` `warning` `error` `err` `none` `off`
以及 `0`..`4`，并容忍首尾空白与引号 —— 是给命令行、MQTT 这类外部输入用的。
名字不认识时返回 `false`，等级保持不变。

每次切换成功都会打一条回显：

```
14:32:07 I [pump_display.XLOG]: log level -> DEBUG
```

这条回显固定按 INFO 打，而且**不受等级过滤**：否则切到 WARN 以上时，确认信息
会被刚设上的等级自己挡掉，看起来像命令没生效。

### 6. 从 MQTT / 命令行切换等级

**库里没有 MQTT，也不认识任何报文格式** —— 它只认等级字符串。报文长什么样、
topic 怎么分，是工程自己的事，在工程侧取出来再调 `setLevelByName()` 即可：

```cpp
// 工程侧 MQTT 回调里，解析出 JSON 之后
if (obj["log_level"].is<String>())
{
    if (!ExtLogger::instance().setLevelByName(obj["log_level"]))
        XLOGW("MQTT", "unknown log level: %s", obj["log_level"].as<const char *>());
    return;
}
```

```bash
# 对应的报文：一条指令换一个等级
mosquitto_pub -h 192.168.0.20 -t hass/circ_pump/cmd -m '{"log_level":"debug"}'
```

本工程的接法见 `src/mqtt2.cpp` 的 `MqttManager::handleCmd()`。

### 7. 注意事项

- **不要在中断里调用**：内部会取互斥量、调 `vsnprintf`，都不是 ISR 安全的。
- **`loop()` 要周期调用**：不调用的话，断网期间缓冲的 UDP 日志会一直压在缓冲里。
- **缓冲满时丢最旧的一整行**：保留最新现场比保留最旧的有用。只开 UDP 不开串口
  时，UDP 未就绪期间的所有日志都要占缓冲。
- **单行超长会被截断**（`vsnprintf` 语义），不会撑爆缓冲区。
- 关掉 UDP 后工程里要用 `#if EXTLOGGER_ENABLE_UDP` 包住 `enableUDP()` 调用。

### 8. 从 v1.x 升级

| 变化                                               | 说明                                                                                                |
| -------------------------------------------------- | --------------------------------------------------------------------------------------------------- |
| 日志行多了等级字母                                 | `12:00:00 [dev.TAG]: msg` → `12:00:00 I [dev.TAG]: msg`，解析日志的脚本要跟着改                     |
| `log(tag, fmt, ...)` → `log(Level, tag, fmt, ...)` | 直接用宏的话不受影响；宏之外几乎不该直接调 `log()`                                                  |
| 新增                                               | `setLevel()` / `setLevelByName()` / `levelName()` / `enabled()`，切换成功会打一条不受等级过滤的回显 |
| 新增                                               | `XLOGD/XLOGI/XLOGW/XLOGE`，`XLOG` 仍等价于 `XLOGI`                                                  |
| 主机名默认值                                       | 由未初始化改为 `"ESP32"`，不再是随机内容                                                            |

---

## 二、库的设计逻辑

### 为什么是单例 + 宏

日志点散布在各个模块里，如果要求把 logger 对象一路传下去，每个函数签名都得为它
让路。单例 + 宏让调用点只剩一行，而且宏里能做“等级不够就整个跳过”的短路，
这是普通函数调用做不到的（参数求值和格式化都会先发生）。

### 格式串交给编译器检查

`log()` 声明上带 `__attribute__((format(printf, 4, 5)))`，所有 XLOG 宏最终都落到
它上面，于是每一处日志的格式串和参数都会被编译器按 printf 的规则检查。这条
不能省：日志是“出问题时才看”的东西，格式写错往往要等到那一刻才暴露，而那时
你正指望它给出线索。

### 两条通道的语义不一样

- **串口**是调试口：直出、阻塞。调试时希望日志立刻出现，而且串口写阻塞这件事
  本身就是信号 —— 一旦阻塞说明日志量超过了串口带宽。串口不做缓冲也是有意的：
  真出了死循环，缓冲只会把内存吃光，日志反而看不到了。
- **UDP** 是运行期观测口：非阻塞。WiFi 断了不能把主循环卡住，所以断网期间写进
  环形缓冲，重连后由 `loop()` 补发。补发动作放在 `loop()` 而不是 `log()` 里，
  是为了让写日志的路径永远不被网络拖住。

### 环形缓冲的几个取舍

- **按行丢弃，不按字节**：半行日志没有意义，读出来只会让接收端多一行乱码。
- **读写指针相等表示空**，所以实际容量是 `BUFFER_SIZE - 1` 字节。
- **`popLine` 一次最多读 `MAX_LINE-1` 字节**：所以 `pushLine` 拒绝比一行还长的
  字符串（而不是截断存放），否则读出来会被腰斩成两行。
- **只发一个 UDP 报文**：补发的历史 + 当前这条攒进同一个 `beginPacket()`，
  接收端看到的顺序才和产生顺序一致。

### 等级为什么这么做

- **判定放在宏里**（`enabled()` 短路），被滤掉的行连 `vsnprintf` 都不执行 ——
  否则“省日志”只省了 I/O，CPU 该花还是花。
- **编译期下限 `EXTLOGGER_MIN_LEVEL`** 把低于它的宏整句替换成 `((void)0)`，
  连格式串都不占 Flash。固件体积敏感时这是唯一真正有效的办法。
- **等级用单调递增的整数**：比较就是一次 `>=`，不需要查表。

### 锁的粒度

只有 `log()`、`loop()`、通道开关这三个入口取锁，粒度粗但简单。看起来嵌套调用
（比如 `enableSerial()` 里接着写一条日志）有死锁风险，实际上 FreeRTOS 的互斥量
对**同一个任务**是可重入的，且这些函数一律是“先解锁再写日志”，两道保险。

`EXTLOGGER_ENABLE_LOCK=0` 时 `lock()/unlock()` 是空函数，单任务工程可以省掉
每次取锁的开销；代价是那时它就不是多任务安全的了。

ISR 里绝对不要调用 —— 会取锁，而中断上下文不能阻塞。

### 时间戳为什么判断年份

`time(NULL)` 在没同步过 SNTP 时返回 1970，直接格式化会打出一串编造的时间，
排错时把人带偏。所以只在“年份 > 2020”时才用真实时间，否则退回 `millis()`。
不用 Arduino 的 `getLocalTime()`：它内部带 `delay(10)`，而且超时传 0 时会因为
`millis()` 已经走了一毫秒而直接跳过检查。

### 等级输入为什么只认字符串

库不去连 MQTT、不订阅 topic、也不解析 JSON，只用一个 `setLevelByName()` 接收
等级字符串。两个理由：

- 让库自己收报文，就得把 broker 地址、clientId、回调所有权一并塞进一个“日志
  库”里，这跟“拷到 lib/ 就能用”是矛盾的；报文长什么样是工程自己的约定，
  每个工程都不一样。
- 为了读一个字段而往库里拉一个 JSON 依赖，更不划算。

代价是工程侧要多写三行（见“使用方法”第 6 节），换来的是这个库在任何工程里
都不需要额外依赖。

### 回显为什么要绕开等级过滤

切换等级的回显如果走普通的 `log()`，就要被“刚设上的那个等级”判一次：切到
WARN、ERROR、NONE 时，确认信息自己先被挡掉，现象是“指令发出去了，日志里什么
都没有”，分不清是没生效还是被过滤了。所以回显走 `emit()` 直接发，并固定按
INFO 打 —— 它必须是一次可预期的确认，而不是一条看运气的日志。

### 常驻开销

默认配置（ESP32、UDP 打开）下，本模块的静态内存就是那个 1KB 环形缓冲加上几十
字节的成员；关掉 UDP 后连这 1KB 也没有了。它不额外持有堆内存，日志宏在等级
不够时不产生任何运行开销。
