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

#define TOUCH_UP    4
#define TOUCH_DOWN  5
#define TOUCH_SHIFT 6
#define TOUCH_OK    7
#define TOUCH_START 8

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

long last_millis = 0;

Screen  scr;
Button  btnUp;
Button  btnDown;
Button  btnOK;
Button  btnShift;
Button  btnStart;
Button *buttons[5] = {&btnUp, &btnDown, &btnOK, &btnShift, &btnStart};

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
    btnUp.setTouchPin(TOUCH_UP);

    btnDown.setCallbackClick([]() { scr.onDown(); });
    btnDown.setTouchPin(TOUCH_DOWN);

    btnOK.setCallbackClick([]() { scr.onOK(); });
    btnOK.setTouchPin(TOUCH_OK);

    btnShift.setCallbackClick([]() { scr.onShift(); });
    btnShift.setTouchPin(TOUCH_SHIFT);

    btnStart.setCallbackClick([]() { scr.onStart(); });
    btnStart.setTouchPin(TOUCH_START);
}

void loop(void)
{
    // 整体循环频率约60Hz

    // OTA监控需要频繁调用
    ota.handle();

    for (int i = 0; i < 5; i++)
        buttons[i]->stateTick(touchRead(buttons[i]->getTouchPin()));

    scr.draw(); // 4个屏幕刷新共约16ms

    if (millis() - last_millis >= 1000)
    {
        Serial.println("o");
        last_millis = millis();
    }
}