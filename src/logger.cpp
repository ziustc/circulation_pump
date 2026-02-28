#include "Logger.h"
#include <Arduino.h>
#include <stdarg.h>

Logger::Logger(uint16_t port)
: server(port)
, mutex(nullptr)
{
}

void Logger::begin()
{
    server.begin();
    server.setNoDelay(true);
    mutex = xSemaphoreCreateMutex();
}

void Logger::loop()
{
    // 若wifi未连接，则断开client并返回
    if (WiFi.status() != WL_CONNECTED)
    {
        if (client && client.connected()) client.stop();
        return;
    }

    // 若已有client但断开，释放
    if (client && !client.connected()) client.stop();

    // 仅允许一个client
    if (!client || !client.connected())
    {
        WiFiClient newClient = server.available();
        if (newClient) client = newClient;
    }
}

void Logger::printf(const char *format, ...)
{
    char buffer[512];

    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    sendOutput(buffer);
}

void Logger::println(const char *msg)
{
    sendOutput(msg);
    sendOutput("\r\n");
}

void Logger::print(const char *msg) { sendOutput(msg); }

void Logger::sendOutput(const char *msg)
{
    if (mutex) xSemaphoreTake(mutex, portMAX_DELAY);

    // 若WiFi已连接且client有效，则发送到telnet
    if (WiFi.status() == WL_CONNECTED && client && client.connected()) client.print(msg);

    // 同时发送到串口
    Serial.print(msg);

    if (mutex) xSemaphoreGive(mutex);
}

Logger logger(23);