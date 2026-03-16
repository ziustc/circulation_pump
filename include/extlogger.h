#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define LOGGER_BUFFER_SIZE 1024
#define LOGGER_MAX_LINE    256

class ExtLogger
{
public:
    static ExtLogger &instance();
    void              begin();

    /* Sink enable */
    void enableSerial(uint32_t baud = 115200);
    void enableUDP(const char *ipStr, uint16_t port);
    void enableTelnet(uint16_t port = 23);

    void log(const char *tag, const char *fmt, ...);

private:
    ExtLogger();
    static void taskEntry(void *arg);
    void        taskLoop();
    void        telnetHandleClient();
    void        push(const char *str);
    bool        popLine(char *out);
    bool        tryLock();
    void        serialPrint(const char *str);
    void        udpPrint(const char *str);
    void        telnetPrint(const char *str);

private:
    /* ring buffer */
    uint8_t         buffer[LOGGER_BUFFER_SIZE];
    volatile size_t writePos = 0;
    volatile size_t readPos  = 0;

    SemaphoreHandle_t mutex;
    TaskHandle_t      taskHandle = nullptr;

    bool timeSynced = false;

    /* sinks */
    bool     serialEnabled = false;
    uint32_t serialBaud    = 115200;

    bool      udpEnabled = false;
    WiFiUDP   udp;
    IPAddress udpIP;
    uint16_t  udpPort;

    bool        telnetEnabled = false;
    WiFiServer *telnetServer  = nullptr;
    WiFiClient  telnetClient;
};

#define XLOG(tag, fmt, ...) ExtLogger::instance().log(tag, fmt, ##__VA_ARGS__)