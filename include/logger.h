#pragma once

#include <Arduino.h>
#include <WiFi.h>

class Logger
{
public:
    Logger(uint16_t port = 23);
    void begin();
    void loop();

    void printf(const char *format, ...);
    void println(const char *msg);
    void print(const char *msg);

private:
    WiFiServer        server;
    WiFiClient        client;
    SemaphoreHandle_t mutex;

    void sendOutput(const char *msg);
};

extern Logger logger;