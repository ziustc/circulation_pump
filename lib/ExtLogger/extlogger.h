#pragma once

/*************************************************************
 * ExtLogger 日志模块
 *
 * 提供 printf 风格的日志宏、串口/UDP 两个输出通道、断网时的环形缓冲，
 * 以及可在运行时切换的日志等级。
 *
 * 除了 Arduino 内核和（可选的）WiFi 之外不依赖任何东西：整个目录拷进
 * 工程的 lib/ 下就能用。开关与容量都在下面"编译期配置"一段里，
 * 用 build_flags 覆盖即可，不必改源码。
 *
 * 使用示例：
 *   // setup()中初始化：
 *   ExtLogger::instance().init("HostName", true);
 *
 *   // 启用日志输出通道：
 *   ExtLogger::instance().enableSerial(115200);
 *   ExtLogger::instance().enableUDP("192.168.0.100", 10001);
 *
 *   // 任何地方写日志：
 *   XLOG("TAG", "Formatted log message: %d", value);
 *   XLOGW("TAG", "Something looks wrong: %d", value);
 *
 *   // 运行时切等级（会打一条回显）：
 *   ExtLogger::instance().setLevelByName("debug");
 *
 *   // loop()中定期调用（重连后补发断网期间缓冲的日志）：
 *   ExtLogger::instance().loop();
 *
 * 日志行格式：
 *   hh:mm:ss W [HostName.TAG]: message
 *   时间     等级 主机名   标签  正文
 *
 *   等级是单个字母 D/I/W/E；时间在未开真实时间、或真实时间还没同步上时
 *   显示 millis()，此时不是 8 个字符宽。
 *
 * 详见同目录 README.md。
 *
 * by: ZiUtility
 * v2.0.20261004
 *
 *************************************************************/

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

// ============================== 编译期配置 ==============================
// 每一项都用 #ifndef 包住，工程里用 build_flags 覆盖，例如：
//   build_flags = -DEXTLOGGER_ENABLE_UDP=0 -DEXTLOGGER_BUFFER_SIZE=4096

// 【UDP 通道】依赖 WiFi，默认只在 ESP 平台打开。
// 关掉后 enableUDP()/disableUDP() 不再声明（调用会编译报错，比默默失效好），
// 环形缓冲及相关代码也一并消失，省下 1KB RAM 和一段 Flash。
#ifndef EXTLOGGER_ENABLE_UDP
    #if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
        #define EXTLOGGER_ENABLE_UDP 1
    #else
        #define EXTLOGGER_ENABLE_UDP 0
    #endif
#endif

// 【互斥锁】FreeRTOS 互斥量，允许其它任务和 loop() 同时写日志。
// 默认只在 ESP32 上打开 —— ESP8266 的 Arduino 内核不保证暴露 FreeRTOS 头文件。
// 全程单任务（只在 loop 里写日志）的工程可以关掉，省一次取锁的开销。
#ifndef EXTLOGGER_ENABLE_LOCK
    #if defined(ARDUINO_ARCH_ESP32)
        #define EXTLOGGER_ENABLE_LOCK 1
    #else
        #define EXTLOGGER_ENABLE_LOCK 0
    #endif
#endif

// 【环形缓冲大小】单位字节，只有 UDP 通道用它（WiFi 断开时暂存）。
#ifndef EXTLOGGER_BUFFER_SIZE
    #define EXTLOGGER_BUFFER_SIZE 1024
#endif

// 【单行上限】含时间、主机名、标签和正文的整行长度上限，单位字节。
#ifndef EXTLOGGER_MAX_LINE
    #define EXTLOGGER_MAX_LINE 512
#endif

#if EXTLOGGER_MAX_LINE >= EXTLOGGER_BUFFER_SIZE
    #error "ExtLogger: EXTLOGGER_MAX_LINE 必须小于 EXTLOGGER_BUFFER_SIZE，否则一行都存不下"
#endif

// 【主机名长度】日志行里 [主机名.标签] 一段的上限，单位字节。
#ifndef EXTLOGGER_HOSTNAME_MAX
    #define EXTLOGGER_HOSTNAME_MAX 24
#endif

// 【默认主机名】init() 之前（或干脆没调过 init()）用的名字。
#ifndef EXTLOGGER_DEFAULT_HOSTNAME
    #define EXTLOGGER_DEFAULT_HOSTNAME "ESP32"
#endif

// 【等级】数值越大越严重。这里用宏而不是 enum，是为了让下面的 #if 能用上。
#define EXTLOGGER_LEVEL_DEBUG 0
#define EXTLOGGER_LEVEL_INFO  1
#define EXTLOGGER_LEVEL_WARN  2
#define EXTLOGGER_LEVEL_ERROR 3
#define EXTLOGGER_LEVEL_NONE  4

// 【编译期下限】低于它的日志宏整句不编进固件（连格式串都不占 Flash）。
// 正式版可以设成 EXTLOGGER_LEVEL_INFO，把调试语句从二进制里彻底去掉。
#ifndef EXTLOGGER_MIN_LEVEL
    #define EXTLOGGER_MIN_LEVEL EXTLOGGER_LEVEL_DEBUG
#endif

// 【开机等级】运行时的初始等级，之后可以用 setLevel()/setLevelByName() 改。
#ifndef EXTLOGGER_DEFAULT_LEVEL
    #define EXTLOGGER_DEFAULT_LEVEL EXTLOGGER_LEVEL_INFO
#endif

#if EXTLOGGER_ENABLE_UDP
    #if defined(ARDUINO_ARCH_ESP32)
        #include <WiFi.h>
    #elif defined(ARDUINO_ARCH_ESP8266)
        #include <ESP8266WiFi.h>
    #endif
    #include <WiFiUdp.h>
#endif

#if EXTLOGGER_ENABLE_LOCK
    #include <freertos/FreeRTOS.h>
    #include <freertos/semphr.h>
#endif

// 让编译器像检查 printf 一样检查格式串。所有 XLOG 宏最终都落到 log() 上，
// 这一条属性等于给每一处日志都加了编译期检查 —— 少了它，格式串和参数对不上
// 要等运行时打出乱码（甚至读错栈）才发现。
#if defined(__GNUC__)
    #define EXTLOGGER_PRINTF_LIKE(fmt_idx, arg_idx) __attribute__((format(printf, fmt_idx, arg_idx)))
#else
    #define EXTLOGGER_PRINTF_LIKE(fmt_idx, arg_idx)
#endif

class ExtLogger
{
public:
    /** 日志等级，数值越大越严重。低于当前等级的日志直接丢弃。 */
    enum class Level : uint8_t
    {
        DEBUG = EXTLOGGER_LEVEL_DEBUG,
        INFO  = EXTLOGGER_LEVEL_INFO,
        WARN  = EXTLOGGER_LEVEL_WARN,
        ERROR = EXTLOGGER_LEVEL_ERROR,
        NONE  = EXTLOGGER_LEVEL_NONE
    };

    /**
     * 获取日志单例，可直接用ExtLogger::instance().xxxx()的形式调用函数
     * @return ExtLogger& 单例对象引用
     */
    static ExtLogger &instance();

    /**
     * 初始化日志模块，在setup()中调用一次即可
     * @param hostName 日志主机名，用于标识当前设备；传 NULL 或空串则保持原值
     * @param showRealTime 是否显示真实时间，false 时显示 millis()
     */
    void init(const char *hostName, bool showRealTime = false);

    /**
     * 检查WiFi状态，如果WiFi连接，则发送缓冲的UDP日志
     * 应在主循环中定期调用
     */
    void loop();

    /**
     * 启用串口输出，串口为阻塞式输出，时间准确便于调试，但不应长时间输出大量日志以免阻塞主执行流
     * @param baud 串口波特率，默认 115200
     */
    void enableSerial(uint32_t baud = 115200);

    /**
     * 禁用串口输出
     */
    void disableSerial();

#if EXTLOGGER_ENABLE_UDP
    /**
     * 启用 UDP 输出
     * @param ipStr 目标主机 IP 字符串
     * @param port 目标端口
     */
    void enableUDP(const char *ipStr, uint16_t port);

    /**
     * 禁用 UDP 输出，同时环形缓冲区清空
     */
    void disableUDP();
#endif

    /**
     * 设置运行时等级。低于它的日志被丢弃，且不产生格式化开销
     *
     * 切换成功会打一条 "log level -> XXX" 的回显。这条回显固定按 INFO 打、
     * 且不受等级过滤 —— 否则切到 WARN 以上时会被刚设上的等级自己挡掉。
     *
     * @param lv 目标等级
     * @return true 设置成功；false 等级取值非法
     */
    bool setLevel(Level lv);

    /** 取当前运行时等级 */
    Level level() const;

    /**
     * 按名字设置等级，供命令行、MQTT 指令这类外部输入使用。
     *
     * 库里没有 MQTT，也不认识任何报文格式：工程侧从自己的报文里取出字符串
     * 传进来即可（例如 obj["log_level"]）。
     *
     * 认 debug/info/warn/warning/error/err/none/off（不分大小写）和 0..4，
     * 也容忍外部输入常见的首尾空白与引号。
     *
     * @param name 等级名或序号
     * @return true 认得这个名字（并已切换，带回显）；false 不认识，等级保持不变
     */
    bool setLevelByName(const char *name);

    /** 等级名，用于日志和回显 */
    static const char *levelName(Level lv);

    /** 该等级当前是否会被输出。宏里用它做短路，避免白算一遍格式化 */
    bool enabled(Level lv) const;

    /**
     * 写日志，实际使用宏 XLOGD/XLOGI/XLOGW/XLOGE 调用，支持 printf 风格格式化
     * @param lv 本条日志的等级
     * @param tag 日志标签
     * @param fmt printf 风格格式字符串
     */
    void log(Level lv, const char *tag, const char *fmt, ...) EXTLOGGER_PRINTF_LIKE(4, 5);

private:
    ExtLogger();

    void lock();
    void unlock();

    bool anyChannelOn() const;                              // 是否至少开了一个输出通道
    void emit(Level lv, const char *tag, const char *text); // 拼行并分发到各通道，不判等级
    void serialPrint(const char *line);                     // 输出日志到串口
    void formatTime(char *buf, size_t n) const;             // 格式化日志行开头的时间

#if EXTLOGGER_ENABLE_UDP
    void   udpPrint(const char *line);  // 输出日志到 UDP
    void   pushLine(const char *str);   // 将字符串写入环形缓冲区
    bool   popLine(char *out);          // 从环形缓冲区读取一行
    size_t usedSize() const;            // 获取缓冲区已使用大小
    size_t freeSize() const;            // 获取缓冲区剩余大小
    void   discardOldestLine();         // 丢弃最旧的一行日志

    uint8_t         buffer[EXTLOGGER_BUFFER_SIZE]; // 环形缓冲区，仅用于UDP在WiFi断开时
    volatile size_t writePos = 0;                  // 下一次写入位置
    volatile size_t readPos  = 0;                  // 下一次读取位置

    // UDP 输出是否启用。启用时可能还没初始化（WiFi 尚未连上），但已经可以开始缓存日志了
    bool      udpEnabled = false;
    bool      udpInitted = false; // UDP 是否已初始化
    WiFiUDP   udp;                // UDP 输出对象
    IPAddress udpIP;              // UDP 目标 IP
    uint16_t  udpPort = 0;        // UDP 目标端口
#endif

#if EXTLOGGER_ENABLE_LOCK
    SemaphoreHandle_t mutex; // 互斥锁，用于多线程同步
#endif

    bool           serialEnabled = false;                          // 串口输出是否启用
    volatile Level levelValue    = (Level)EXTLOGGER_DEFAULT_LEVEL; // 运行时等级
    char           hostName[EXTLOGGER_HOSTNAME_MAX];               // 设备主机名，用于日志标识
    bool           logRealTime   = false;                          // 显示真实时间（需 SNTP 同步过）
};

// ============================== 日志宏 ==============================
// 等级判定放在宏里做短路：被滤掉的行连 vsnprintf 都不会执行。
// 编译期就被 MIN_LEVEL 关掉的等级，连这段判断都不生成。
#define EXTLOGGER_LOG_AT(level, tag, fmt, ...)                             \
    do                                                                     \
    {                                                                      \
        ExtLogger &logger = ExtLogger::instance();                         \
        if (logger.enabled((ExtLogger::Level)(level)))                     \
        {                                                                  \
            logger.log((ExtLogger::Level)(level), tag, fmt, ##__VA_ARGS__);\
        }                                                                  \
    } while (0)

#if EXTLOGGER_MIN_LEVEL <= EXTLOGGER_LEVEL_DEBUG
    #define XLOGD(tag, fmt, ...) EXTLOGGER_LOG_AT(EXTLOGGER_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#else
    #define XLOGD(tag, fmt, ...) ((void)0)
#endif

#if EXTLOGGER_MIN_LEVEL <= EXTLOGGER_LEVEL_INFO
    #define XLOGI(tag, fmt, ...) EXTLOGGER_LOG_AT(EXTLOGGER_LEVEL_INFO, tag, fmt, ##__VA_ARGS__)
#else
    #define XLOGI(tag, fmt, ...) ((void)0)
#endif

#if EXTLOGGER_MIN_LEVEL <= EXTLOGGER_LEVEL_WARN
    #define XLOGW(tag, fmt, ...) EXTLOGGER_LOG_AT(EXTLOGGER_LEVEL_WARN, tag, fmt, ##__VA_ARGS__)
#else
    #define XLOGW(tag, fmt, ...) ((void)0)
#endif

#if EXTLOGGER_MIN_LEVEL <= EXTLOGGER_LEVEL_ERROR
    #define XLOGE(tag, fmt, ...) EXTLOGGER_LOG_AT(EXTLOGGER_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#else
    #define XLOGE(tag, fmt, ...) ((void)0)
#endif

// 旧名字，等价于 XLOGI。老代码不用改
#define XLOG(tag, fmt, ...) XLOGI(tag, fmt, ##__VA_ARGS__)
