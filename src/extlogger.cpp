#include "extlogger.h"
#include <stdarg.h>
#include <time.h>

ExtLogger &ExtLogger::instance()
{
    static ExtLogger inst;
    return inst;
}

ExtLogger::ExtLogger() { mutex = xSemaphoreCreateMutex(); }

void ExtLogger::init(const char *hostName, bool showRealTime)
{
    snprintf(_hostName, sizeof(_hostName), "%s", hostName);
    _showRealTime = showRealTime;
    xTaskCreatePinnedToCore(taskEntry, "loggerTask", 4096, this, 1, &taskHandle, 1);
}

void ExtLogger::enableSerial(uint32_t baud)
{
    serialEnabled = true;
    serialBaud    = baud;

    Serial.begin(serialBaud);
}

void ExtLogger::enableUDP(const char *ipStr, uint16_t port)
{
    udpEnabled = true;
    udpPort    = port;
    udpIP.fromString(ipStr);

    udp.begin(0);
    log("XLOG", "UDP logger enabled. Target = %s:%d", udpIP.toString().c_str(), udpPort);
}

void ExtLogger::enableTelnet(uint16_t port)
{
    telnetEnabled = true;

    telnetServer = new WiFiServer(port);
    telnetServer->begin();
    log("XLOG", "Telnet logger enabled. Listening on port %d", port);
}

bool ExtLogger::tryLock()
{
    if (xSemaphoreTake(mutex, 0) == pdTRUE) return true;
    return false;
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

void ExtLogger::push(const char *str)
{
    if (!tryLock()) return;

    size_t len = strlen(str);
    if (len >= LOGGER_BUFFER_SIZE)
    {
        xSemaphoreGive(mutex);
        return;
    }

    while (len > freeSize())
    {
        discardOldestLine();
    }

    for (size_t i = 0; i < len; i++)
    {
        buffer[writePos] = str[i];
        writePos         = (writePos + 1) % LOGGER_BUFFER_SIZE;
    }

    xSemaphoreGive(mutex);
}

bool ExtLogger::popLine(char *out)
{
    if (!tryLock()) return false;

    if (readPos == writePos)
    {
        xSemaphoreGive(mutex);
        return false;
    }

    size_t i = 0;
    while (readPos != writePos && i < LOGGER_MAX_LINE - 1)
    {
        char c   = buffer[readPos];
        readPos  = (readPos + 1) % LOGGER_BUFFER_SIZE;
        out[i++] = c;
        if (c == '\n') break;
    }
    out[i] = 0;

    xSemaphoreGive(mutex);
    return true;
}

void ExtLogger::log(const char *tag, const char *fmt, ...)
{
    // 格式化时间
    char      timebuf[32];
    time_t    now = time(NULL);
    struct tm t;

    localtime_r(&now, &t);
    if (_showRealTime && t.tm_year + 1900 > 2020) // Year > 2020表示已经真实时间
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
    snprintf(fullLog, sizeof(fullLog), "%s [%s.%s]: %s\r\n", timebuf, _hostName, tag, userText);

    // 完整日志推送到缓冲区
    push(fullLog);

    // 若enable串口，则直接阻塞方式打印，实时观察程序状态
    if (serialEnabled) serialPrint(fullLog);
}

void ExtLogger::telnetHandleClient()
{
    if (!telnetEnabled) return;
    if (!telnetClient || !telnetClient.connected()) telnetClient = telnetServer->accept();
}

void ExtLogger::taskEntry(void *arg)
{
    ExtLogger *logger = (ExtLogger *)arg;
    logger->taskLoop();
}

void ExtLogger::taskLoop()
{
    char line[LOGGER_MAX_LINE];

    while (true)
    {
        // 处理Telnet客户端连接
        telnetHandleClient();

        // 从缓冲区取出一行日志，发送到各个sink
        if (popLine(line))
        {
            // if (serialEnabled) serialPrint(line); // 串口不走task，直接在log函数里打印
            if (udpEnabled) udpPrint(line);
            if (telnetEnabled) telnetPrint(line);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void ExtLogger::serialPrint(const char *line) { Serial.print(line); }

void ExtLogger::udpPrint(const char *line)
{
    if (WiFi.isConnected())
    {
        udp.beginPacket(udpIP, udpPort);
        udp.write((uint8_t *)line, strlen(line));
        udp.endPacket();
    }
}

void ExtLogger::telnetPrint(const char *line)
{
    if (telnetClient && telnetClient.connected()) telnetClient.print(line);
}