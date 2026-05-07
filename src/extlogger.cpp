#include "extlogger.h"
#include <stdarg.h>
#include <time.h>

ExtLogger &ExtLogger::instance()
{
    static ExtLogger inst;
    return inst;
}

ExtLogger::ExtLogger() { }

void ExtLogger::init(const char *shortName, bool showRealTime)
{
    snprintf(hostName, sizeof(hostName), "%s", shortName);
    logRealTime = showRealTime;
}

void ExtLogger::loop()
{
    char line[LOGGER_MAX_LINE];

    if (udpEnabled && !udpInited && WiFi.isConnected())
        if (udp.begin(0)) udpInited = true;

    if (udpEnabled && udpInited && WiFi.isConnected())
        while (popLine(line))
            udpPrint(line);
}

void ExtLogger::enableSerial(uint32_t baud)
{
    serialEnabled = true;
    serialBaud    = baud;

    Serial.begin(serialBaud);
}

void ExtLogger::disableSerial() { serialEnabled = false; }

void ExtLogger::enableUDP(const char *targetIp, uint16_t port)
{
    udpEnabled = true;
    udpPort    = port;
    udpIP.fromString(targetIp);

    if (WiFi.isConnected())
        if (udp.begin(0)) udpInited = true;

    log("XLOG", "UDP logger enabled. Target = %s:%d", targetIp, port);
}

void ExtLogger::disableUDP()
{
    udp.stop();
    udpEnabled = false;
    udpInited  = false;
    writePos   = 0;
    readPos    = 0;
}

void ExtLogger::log(const char *tag, const char *fmt, ...)
{
    // 格式化时间
    char      timebuf[32];
    time_t    now = time(NULL);
    struct tm t;

    localtime_r(&now, &t);
    if (logRealTime && t.tm_year + 1900 > 2020) // Year > 2020表示已经真实时间
        snprintf(timebuf, sizeof(timebuf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
    else
        snprintf(timebuf, sizeof(timebuf), "%lu", millis());

    // 格式化日志内容
    char    userText[LOGGER_MAX_LINE];
    char    fullLog[LOGGER_MAX_LINE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(userText, sizeof(userText), fmt, args);
    va_end(args);

    // 去掉末尾用户自加的\r,\n,或\r\n
    size_t len = strlen(userText);
    if (len >= 2 && userText[len - 2] == '\r' && userText[len - 1] == '\n')
        userText[len - 2] = '\0';
    else if (userText[len - 1] == '\n' || userText[len - 1] == '\r')
        userText[len - 1] = '\0';

    // 组装为完整log
    snprintf(fullLog, sizeof(fullLog), "%s [%s.%s]: %s\r\n", timebuf, hostName, tag, userText);

    // 串口输出
    if (serialEnabled) serialPrint(fullLog);

    // UDP处理
    if (udpEnabled) udpPrint(fullLog);
}

void ExtLogger::serialPrint(const char *line) { Serial.print(line); }

void ExtLogger::udpPrint(const char *line)
{
    if (udpInited && WiFi.isConnected())
    {
        udp.beginPacket(udpIP, udpPort);
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
    size_t len = strlen(str);
    if (len >= LOGGER_BUFFER_SIZE) return;

    while (len > freeSize())
        discardOldestLine();

    for (size_t i = 0; i < len; i++)
    {
        buffer[writePos] = str[i];
        writePos         = (writePos + 1) % LOGGER_BUFFER_SIZE;
    }
}

bool ExtLogger::popLine(char *out)
{
    if (readPos == writePos) return false;

    size_t i = 0;
    while (readPos != writePos && i < LOGGER_MAX_LINE - 1)
    {
        char c   = buffer[readPos];
        readPos  = (readPos + 1) % LOGGER_BUFFER_SIZE;
        out[i++] = c;
        if (c == '\n') break;
    }
    out[i] = 0;
    return true;
}

size_t ExtLogger::usedSize()
{
    if (writePos >= readPos) return writePos - readPos;
    return LOGGER_BUFFER_SIZE - (readPos - writePos);
}

size_t ExtLogger::freeSize() { return LOGGER_BUFFER_SIZE - usedSize(); }

void ExtLogger::discardOldestLine()
{
    if (readPos == writePos) return;

    while (readPos != writePos)
    {
        char c  = buffer[readPos];
        readPos = (readPos + 1) % LOGGER_BUFFER_SIZE;
        if (c == '\n') break;
    }
}
