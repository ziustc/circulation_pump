#pragma once

/*************************************************************
 * ExtLogger 日志模块
 *
 * 本库提供日志缓冲、串口/UDP 输出。
 * 日志通过环形缓冲区缓存（仅UDP在WiFi断开时使用），避免输出阻塞主执行流。
 *
 * 使用示例：
 *   // setup()中初始化：
 *   ExtLogger::instance().init("HostName", true);
 * 
 *   // 启用日志输出通道：
 *   ExtLogger::instance().enableSerial(115200);
 *   ExtLogger::instance().enableUDP("192.168.0.100", 514);
 * 
 *   // 任何地方写日志：
 *   XLOG("TAG", "Formatted log message: %d", value);
 * 
 * by: ZiUtility
 * v1.3.20260506
 * 
 *************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define LOGGER_BUFFER_SIZE 1024 // 环形缓冲区总大小，单位字节
#define LOGGER_MAX_LINE    512  // 单条日志最大长度，单位字节，必须小于 LOGGER_BUFFER_SIZE

class ExtLogger
{
public:
    /**
     * 获取日志单例，可直接用ExtLogger::instance().xxxx()的形式调用函数
     * @return ExtLogger& 单例对象引用
     */
    static ExtLogger &instance();

    /**
     * 初始化日志模块，在setup()中调用一次即可
     * @param hostName 日志主机名，用于标识当前设备
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

    /**
     * 写日志，实际使用宏XLOG(tag, fmt, ...)调用，支持 printf 风格格式化
     * @param tag 日志标签
     * @param fmt printf 风格格式字符串
     */
    void log(const char *tag, const char *fmt, ...);

private:
    uint8_t         buffer[LOGGER_BUFFER_SIZE]; // 环形缓冲区，仅用于UDP在WiFi断开时
    volatile size_t writePos = 0;               // 下一次写入位置
    volatile size_t readPos  = 0;               // 下一次读取位置

    bool      serialEnabled = false;  // 串口输出是否启用
    uint32_t  serialBaud    = 115200; // 串口波特率
    bool      udpEnabled    = false;  // UDP 输出是否启用，启用时可能还没初始化，但已经可以开始缓存日志了
    bool      udpInited     = false;  // UDP 是否已初始化
    WiFiUDP   udp;                    // UDP 输出对象
    IPAddress udpIP;                  // UDP 目标 IP
    uint16_t  udpPort;                // UDP 目标端口

    char hostName[20];        // 设备主机名，用于日志标识
    bool logRealTime = false; // 是否显示真实时间，默认为 false 显示 millis()
    bool timeSynced  = false; // 是否已同步真实时间

    ExtLogger();                         // 私有构造函数，保证单例模式
    void   serialPrint(const char *str); // 输出日志到串口
    void   udpPrint(const char *str);    // 输出日志到 UDP
    void   pushLine(const char *str);    // 将字符串写入环形缓冲区
    bool   popLine(char *out);           // 从环形缓冲区读取一行
    size_t usedSize();                   // 获取缓冲区已使用大小
    size_t freeSize();                   // 获取缓冲区剩余大小
    void   discardOldestLine();          // 丢弃最旧的一行日志
};

#define XLOG(tag, fmt, ...) ExtLogger::instance().log(tag, fmt, ##__VA_ARGS__)