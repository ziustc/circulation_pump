#include <Arduino.h>
#include <WiFi.h>
#include "ota.h"
#include "ssid.h"
#include <stdio.h>
#include <U8g2lib.h>
#include "screen.h"
#include "panel.h"
#include "pattern.h"
#include "button.h"

using namespace std;

OTA  ota;
long last_millis = 0;

// 让 printf 输出到 Serial
extern "C" int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        Serial.write(ptr[i]);
    }
    return len;
}

Screen scr;
Button btnUp;
Button btnDown;
Button btnOK;
Button btnShift;
Button btnStart;

void setup(void)
{
    Serial.begin(115200);
    Serial.println("Circulation Pump Reboot");

    // 连接 WiFi
    Serial.println("WIFI Connecting...");
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
    WiFi.begin(SSID, PASSWORD);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Connection Failed! Rebooting...");
        delay(1000);
        ESP.restart();
    }
    Serial.println("success");

    // 准备OTA
    ota.setHostname(HOSTNAME);
    ota.init();

    // 面板初始化
    btnUp.setCallbackClick([]() { scr.onUp(); });
    btnDown.setCallbackClick([]() { scr.onDown(); });
    btnOK.setCallbackClick([]() { scr.onOK(); });
    btnShift.setCallbackClick([]() { scr.onShift(); });
}

void loop(void)
{
    if (millis() - last_millis >= 1000)
    {
        last_millis = millis();
        Serial.println("loop");
    }

    // 必须保持频繁调用
    ota.handle();

    scr.draw();
}