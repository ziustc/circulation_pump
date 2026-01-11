#include <Arduino.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "ota.h"
#include "ssid.h"

OTA::OTA()
{
    otaPort = 3232; // 端口默认为 3232
}

void OTA::setPort(uint16_t port) { otaPort = port; }

void OTA::setHostname(const char *hostname) { otaHostname = (char *)hostname; }

void OTA::init(void)
{
    // 初始化 ArduinoOTA 配置
    ArduinoOTA.setPort(otaPort);

    // 设置在局域网中显示的设备名
    ArduinoOTA.setHostname(otaHostname);

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";
        Serial.println("Start updating " + type);
    });

    ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });

    ArduinoOTA.onProgress(
        [](unsigned int progress, unsigned int total) { Serial.printf("Progress: %u%%\r", (progress * 100 / total)); });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
    });

    ArduinoOTA.begin();
}

/**
 * 需要在主循环中频繁调用
 */
void OTA::handle(void) { ArduinoOTA.handle(); }