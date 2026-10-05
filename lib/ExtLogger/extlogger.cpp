#include "extlogger.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ============================== 内部小工具 ==============================

// 等级对应的单字符标记，日志行里放在时间后面
static char levelChar(ExtLogger::Level lv)
{
    switch (lv)
    {
        case ExtLogger::Level::DEBUG: return 'D';
        case ExtLogger::Level::INFO:  return 'I';
        case ExtLogger::Level::WARN:  return 'W';
        case ExtLogger::Level::ERROR: return 'E';
        default:                      return '-';
    }
}

// 去掉调用方自己加的 \r \n。日志行统一由本模块用 \r\n 收尾，
// 不处理的话串口上会多出空行、UDP 那边则是一条报文里塞两行。
//
// 用 while 循环而不是只判断最后两个字符：只判断的话，空串会去读 buf[-1]。
static void stripEol(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n'))
    {
        s[--len] = '\0';
    }
}

// ============================== 生命周期 ==============================

ExtLogger &ExtLogger::instance()
{
    static ExtLogger inst;
    return inst;
}

ExtLogger::ExtLogger()
{
    snprintf(hostName, sizeof(hostName), "%s", EXTLOGGER_DEFAULT_HOSTNAME);

#if EXTLOGGER_ENABLE_LOCK
    mutex = xSemaphoreCreateMutex();
#endif
}

#if EXTLOGGER_ENABLE_LOCK
void ExtLogger::lock()
{
    if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);
}

void ExtLogger::unlock()
{
    if (mutex) xSemaphoreGive(mutex);
}
#else
void ExtLogger::lock() {}
void ExtLogger::unlock() {}
#endif

void ExtLogger::init(const char *name, bool showRealTime)
{
    lock();

    if (name && name[0]) snprintf(hostName, sizeof(hostName), "%s", name);
    logRealTime = showRealTime;

    unlock();
}

void ExtLogger::loop()
{
#if EXTLOGGER_ENABLE_UDP
    lock();

    // 断网期间的日志都压在环形缓冲里，WiFi 一回来就补发。
    // 补发放在 loop() 而不是 log() 里做，写日志的路径就不会被网络拖住。
    if (udpEnabled && WiFi.isConnected())
    {
        if (!udpInitted && udp.begin(0)) udpInitted = true;
        if (udpInitted) udpPrint(""); // 空行，只为触发补发
    }

    unlock();
#endif
}

// ============================== 输出通道 ==============================

void ExtLogger::enableSerial(uint32_t baud)
{
    Serial.begin(baud);

    lock();
    serialEnabled = true;
    unlock();

    // 放在启用之后，这条自己才能被看到
    XLOGI("XLOG", "Serial logger enabled. Baud = %lu", (unsigned long)baud);
}

void ExtLogger::disableSerial()
{
    lock();
    serialEnabled = false;
    unlock();
}

#if EXTLOGGER_ENABLE_UDP
void ExtLogger::enableUDP(const char *ipStr, uint16_t port)
{
    lock();

    udpEnabled = true;
    udpPort    = port;
    udpIP.fromString(ipStr);

    if (WiFi.isConnected() && udp.begin(0)) udpInitted = true;

    unlock();

    XLOGI("XLOG", "UDP logger enabled. Target = %s:%u", ipStr, (unsigned)port);
}

void ExtLogger::disableUDP()
{
    lock();

    udp.stop();
    udpEnabled = false;
    udpInitted = false;
    writePos   = 0;
    readPos    = 0;

    unlock();
}
#endif

// ============================== 等级 ==============================

bool ExtLogger::setLevel(Level lv)
{
    if (lv < Level::DEBUG || lv > Level::NONE) return false;

    levelValue = lv;

    // 回显固定按 INFO 打，并且走 emit() 绕开等级判断：
    // 否则切到 WARN 以上时，这条确认会被刚设上的等级自己挡掉，
    // 看起来像命令没生效（切到 NONE 时更是必然看不见）。
    char buf[24];
    snprintf(buf, sizeof(buf), "log level -> %s", levelName(lv));
    emit(Level::INFO, "XLOG", buf);

    return true;
}

ExtLogger::Level ExtLogger::level() const { return levelValue; }

const char *ExtLogger::levelName(Level lv)
{
    switch (lv)
    {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO:  return "INFO";
        case Level::WARN:  return "WARN";
        case Level::ERROR: return "ERROR";
        default:           return "NONE";
    }
}

bool ExtLogger::enabled(Level lv) const
{
    return lv >= levelValue && lv >= (Level)EXTLOGGER_MIN_LEVEL;
}

bool ExtLogger::setLevelByName(const char *name)
{
    if (!name) return false;

    // 外部送来的值常带引号和空白（"\r\ndebug\" " 这种），先剥掉
    while (*name == ' ' || *name == '\t' || *name == '"' || *name == '\'')
        ++name;

    char   buf[16];
    size_t i = 0;
    while (name[i] && i < sizeof(buf) - 1 && name[i] != '"' && name[i] != '\'' && name[i] != '\r' && name[i] != '\n')
    {
        buf[i] = (char)tolower((unsigned char)name[i]);
        ++i;
    }
    buf[i] = '\0';

    while (i > 0 && (buf[i - 1] == ' ' || buf[i - 1] == '\t'))
    {
        buf[--i] = '\0';
    }

    Level lv;
    if (!strcmp(buf, "debug") || !strcmp(buf, "0")) lv = Level::DEBUG;
    else if (!strcmp(buf, "info") || !strcmp(buf, "1")) lv = Level::INFO;
    else if (!strcmp(buf, "warn") || !strcmp(buf, "warning") || !strcmp(buf, "2")) lv = Level::WARN;
    else if (!strcmp(buf, "error") || !strcmp(buf, "err") || !strcmp(buf, "3")) lv = Level::ERROR;
    else if (!strcmp(buf, "none") || !strcmp(buf, "off") || !strcmp(buf, "4")) lv = Level::NONE;
    else return false;

    return setLevel(lv);
}

// ============================== 写日志 ==============================

bool ExtLogger::anyChannelOn() const
{
    bool on = serialEnabled;
#if EXTLOGGER_ENABLE_UDP
    on = on || udpEnabled;
#endif
    return on;
}

void ExtLogger::log(Level lv, const char *tag, const char *fmt, ...)
{
    // 两道过滤都放最前面：被滤掉的行连格式化都不做。
    //
    // 通道开关这里不加锁地读一下就够了 —— 它只在 setup 里翻动，万一读到
    // 旧值，最坏是多格式化一条或丢一条，不影响正确性。
    if (!enabled(lv) || !anyChannelOn()) return;

    char    text[EXTLOGGER_MAX_LINE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt ? fmt : "", args);
    va_end(args);

    emit(lv, tag, text);
}

// 拼出整行并分发到各通道。【不做等级判断】—— 等级过滤在 log() 里，
// 这里也放一道的话，setLevel() 的回显就会被刚设上的等级挡掉。
void ExtLogger::emit(Level lv, const char *tag, const char *text)
{
    lock();

    if (!anyChannelOn())
    {
        unlock();
        return;
    }

    char timebuf[16];
    formatTime(timebuf, sizeof(timebuf));

    // 拷一份再处理：stripEol 要就地改，而 text 可能是常量字符串
    char body[EXTLOGGER_MAX_LINE];
    snprintf(body, sizeof(body), "%s", text ? text : "");
    stripEol(body);

    // 分两段拼：先写前缀（时间/等级/来源），再接正文。
    // 用一次 snprintf 拼完的话，"正文上限 + 前缀上限"必然大于行缓冲，
    // GCC 的 -Wformat-truncation 会为这个注定的截断一直报警告。
    char line[EXTLOGGER_MAX_LINE];
    int  n = snprintf(line, sizeof(line), "%s %c [%s.%s]: ", timebuf, levelChar(lv), hostName, tag ? tag : "-");
    if (n < 0) n = 0;
    if ((size_t)n > sizeof(line) - 1) n = (int)sizeof(line) - 1; // 前缀自己就超长时截断

    strncat(line, body, sizeof(line) - 1 - (size_t)n);
    strncat(line, "\r\n", sizeof(line) - 1 - strlen(line));

    if (serialEnabled) serialPrint(line);
#if EXTLOGGER_ENABLE_UDP
    if (udpEnabled) udpPrint(line);
#endif

    unlock();
}

void ExtLogger::serialPrint(const char *line) { Serial.print(line); }

void ExtLogger::formatTime(char *buf, size_t n) const
{
    if (logRealTime)
    {
        time_t    now = time(NULL);
        struct tm t;
        localtime_r(&now, &t);

        // 年份 > 2020 才算真的同步过时间。没同步时 time() 给的是 1970，
        // 打一个编造的时间戳比打 millis() 更糟 —— 排错时会被它带偏。
        if (t.tm_year + 1900 > 2020)
        {
            snprintf(buf, n, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
            return;
        }
    }

    snprintf(buf, n, "%lu", (unsigned long)millis());
}

// ============================== 环形缓冲（仅 UDP） ==============================

#if EXTLOGGER_ENABLE_UDP
void ExtLogger::udpPrint(const char *line)
{
    char prevLine[EXTLOGGER_MAX_LINE];

    if (udpInitted && WiFi.isConnected())
    {
        udp.beginPacket(udpIP, udpPort);

        // 先补发断网期间攒下的，再发当前这条。攒进同一个报文发出去，
        // 接收端看到的顺序才和产生顺序一致。
        while (popLine(prevLine))
        {
            udp.write((uint8_t *)prevLine, strlen(prevLine));
        }

        udp.write((uint8_t *)line, strlen(line));
        udp.endPacket();
    }
    else
    {
        pushLine(line);
    }
}

void ExtLogger::pushLine(const char *str)
{
    const size_t len = strlen(str);

    // 比一行还长的整条丢掉：popLine 一次最多读 MAX_LINE-1 字节，
    // 存进来的行若比这还长，读出去会被腰斩成两行
    if (len >= EXTLOGGER_MAX_LINE) return;

    while (len > freeSize())
    {
        discardOldestLine();
    }

    for (size_t i = 0; i < len; i++)
    {
        buffer[writePos] = str[i];
        writePos         = (writePos + 1) % EXTLOGGER_BUFFER_SIZE;
    }
}

bool ExtLogger::popLine(char *out)
{
    if (readPos == writePos) return false;

    size_t i = 0;
    while (readPos != writePos && i < EXTLOGGER_MAX_LINE - 1)
    {
        const char c = buffer[readPos];
        readPos      = (readPos + 1) % EXTLOGGER_BUFFER_SIZE;
        out[i++]     = c;
        if (c == '\n') break;
    }
    out[i] = '\0';
    return true;
}

size_t ExtLogger::usedSize() const
{
    if (writePos >= readPos) return writePos - readPos;

    return EXTLOGGER_BUFFER_SIZE - (readPos - writePos);
}

size_t ExtLogger::freeSize() const { return EXTLOGGER_BUFFER_SIZE - usedSize(); }

void ExtLogger::discardOldestLine()
{
    if (readPos == writePos) return;

    while (readPos != writePos)
    {
        const char c = buffer[readPos];
        readPos      = (readPos + 1) % EXTLOGGER_BUFFER_SIZE;
        if (c == '\n') break;
    }
}
#endif
