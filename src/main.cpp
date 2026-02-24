#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <U8g2lib.h>
#include "ota.h"
#include "ssid.h"
#include "screen.h"
#include "panel.h"
#include "pattern.h"
#include "button.h"
#include "controller.h"

#define PUMP_PIN    16
#define TEMP_PIN    17
#define FLOW_PIN    18
#define TOUCH_UP    4
#define TOUCH_DOWN  5
#define TOUCH_SHIFT 6
#define TOUCH_OK    7
#define TOUCH_START 8

using namespace std;

OtaAssist ota; // 我自己的OTA类

long lastMillis = 0;

Screen       scr;
Button       btnUp;
Button       btnDown;
Button       btnOK;
Button       btnShift;
Button       btnStart;
Button      *buttons[5] = {&btnUp, &btnDown, &btnOK, &btnShift, &btnStart};
PumpCtrlUnit ptu(scr, PUMP_PIN, TEMP_PIN, FLOW_PIN);
void         initButton();
void         reportSettings(Settings ctrlSet);
void         timeCalibNotice(struct timeval *tv);

void setup(void)
{
    // 初始化引脚
    pinMode(39, OUTPUT);
    pinMode(40, OUTPUT);
    pinMode(41, OUTPUT);
    pinMode(42, OUTPUT);
    pinMode(9, OUTPUT);
    pinMode(10, OUTPUT);
    pinMode(14, OUTPUT);
    pinMode(21, OUTPUT);

    // 以便让串口助手来得及显示ESP32发来的串口提示信息
    delay(1000);

    // 初始化串口
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

    // 同步真实时间
    configTime(8 * 3600, 0, NTP_SERVER);
    sntp_set_time_sync_notification_cb(timeCalibNotice);

    // 准备显示屏
    scr.init();
    scr.setExportSettings_cb(reportSettings);

    // 按键初始化
    initButton();
}

void loop(void)
{
    static int countOfDraw = 0;
    int        wifiRssi    = WiFi.RSSI();
    float      fps;

    // 整体循环频率约60Hz

    // OTA监控需要频繁调用
    ota.handle();

    // 泵控制单元
    ptu.ctrlTick();

    // 按键扫描
    for (int i = 0; i < 5; i++)
        buttons[i]->stateTick(touchRead(buttons[i]->getTouchPin()));

    scr.updateSignal(wifiRssi);
    fps = scr.draw(); // 4个屏幕刷新共约16ms

    // 确认若已稳定运行则保持本固件，否则重启后回滚到上次稳定固件
    if (!ota.isStable()) countOfDraw++;
    if (!ota.isStable() && countOfDraw > 999)
    {
        ota.confirm();
    }

    if (millis() - lastMillis >= 1000)
    {
        Serial.printf("fps = %.1f\r\n", fps);
        lastMillis = millis();
    }
}

void initButton()
{
    btnUp.setTouchPin(TOUCH_UP);
    btnUp.setLongPressEnable(true);
    btnUp.setCallbackClick([]() { scr.onUp(); });

    btnDown.setTouchPin(TOUCH_DOWN);
    btnDown.setLongPressEnable(true);
    btnDown.setCallbackClick([]() { scr.onDown(); });

    btnOK.setTouchPin(TOUCH_OK);
    btnOK.setCallbackClick([]() { scr.onOK(); });

    btnShift.setTouchPin(TOUCH_SHIFT);
    btnShift.setCallbackClick([]() { scr.onShift(); });

    btnStart.setTouchPin(TOUCH_START);
    btnStart.setCallbackClick([]() { scr.onStart(); });
}

void reportSettings(Settings set)
{
    // 这里应使用MQTT将数据发送出去
    Serial.println("report");
}

void timeCalibNotice(struct timeval *tv) { Serial.println("sntp time calibrated."); }