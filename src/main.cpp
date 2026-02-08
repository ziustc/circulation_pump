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

// 让 printf 输出到 Serial
extern "C" int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        Serial.write(ptr[i]);
    }
    return len;
}

OtaAssist ota; // 我自己的OTA类

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
    // 以便让串口助手来得及显示ESP32发来的串口提示信息
    delay(1000);

    Serial.begin(115200);
    Serial.println("Circulation Pump Reboot");

    // 连接 WiFi
    Serial.print("WIFI Connecting...");
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(HOSTNAME);
    WiFi.begin(SSID, PASSWORD);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        Serial.println("Connection Failed! Rebooting...");
        delay(1000);
        ESP.restart();
    }
    Serial.print("success. IP = ");
    Serial.println(WiFi.localIP());

    // 准备OTA
    ota.init();

    // 若是OTA升级，需要注释掉本条
    // ota.clearOtaData();

    // 准备显示屏
    scr.init();

    // 面板初始化
    // btnUp.setCallbackClick([]() { scr.onUp(); });
    // btnUp.setTouchPin(TOUCH_UP);

    // btnDown.setCallbackClick([]() { scr.onDown(); });
    // btnDown.setTouchPin(TOUCH_DOWN);

    // btnOK.setCallbackClick([]() { scr.onOK(); });
    // btnOK.setTouchPin(TOUCH_OK);

    // btnShift.setCallbackClick([]() { scr.onShift(); });
    // btnShift.setTouchPin(TOUCH_SHIFT);

    // btnStart.setCallbackClick([]() { scr.onStart(); });
    // btnStart.setTouchPin(TOUCH_START);
}

void loop(void)
{
    static int countOfDraw = 0;
    int        wifiRssi    = WiFi.RSSI();
    float      fps;

    // 整体循环频率约60Hz

    // OTA监控需要频繁调用
    ota.handle();

    // for (int i = 0; i < 5; i++)
    //     buttons[i]->stateTick(touchRead(buttons[i]->getTouchPin()));

    scr.updateSignal(wifiRssi);
    fps = scr.draw(); // 4个屏幕刷新共约16ms


    // 确认若已稳定运行则保持本固件，否则重启后回滚到上次稳定固件
    if (!ota.isStable()) countOfDraw++;
    if (!ota.isStable() && countOfDraw > 999)
    {
        ota.confirm();
    }

    if (millis() - last_millis >= 1000)
    {
        Serial.printf("fps = %.1f\r\n", fps);
        last_millis = millis();
    }
}