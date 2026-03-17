#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <U8g2lib.h>
#include "httpota.h"
#include "extlogger.h"
#include "mqtt.h"

#include "ssid.h"
#include "screen.h"
#include "panel.h"
#include "pattern.h"
#include "button.h"
#include "pcu.h"

#define PUMP_PIN    16
#define TEMP_PIN    17
#define FLOW_PIN    18
#define TOUCH_UP    4
#define TOUCH_DOWN  5
#define TOUCH_SHIFT 6
#define TOUCH_OK    7
#define TOUCH_START 8

using namespace std;

long lastMillis = 0;

OtaAssist       ota(80);
PumpMqttManager mqtt;

Screen       scr;
Button       btnUp, btnDown, btnOK, btnShift, btnStart;
Button      *buttons[5] = {&btnUp, &btnDown, &btnOK, &btnShift, &btnStart};
PumpCtrlUnit ptu(scr, PUMP_PIN, TEMP_PIN, FLOW_PIN);

void templateOfSetup();
void timeCalibNotice(struct timeval *tv);
void initPin();
void initButton();
void reportSettings(Settings ctrlSet);

void setup(void)
{
    // 基础配置模板
    templateOfSetup();

    // 初始化引脚
    initPin();
    XLOG("System", "Pins initialized.");

    // 准备显示屏
    scr.init();
    scr.setExportSettings_cb(reportSettings);
    XLOG("System", "Screen initialized.");

    // 按键初始化
    initButton();
    XLOG("System", "Buttons initialized.");

    // 初始化MQTT
    mqtt.begin();
}

void loop(void)
{
    static int countOfDraw = 0;
    int        wifiRssi    = WiFi.RSSI();
    float      fps;

    // 整体循环频率约60Hz

    // OTA监控需要频繁调用
    ota.loop();

    // MQTT
    mqtt.loop();

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

    unsigned long now = millis();
    if (now - lastMillis >= 1000)
    {
        // mqtt.sendMsg("homeassistant/pump/config", "信息");
        XLOG("System", "正常运行中... uptime = %lus, FPS = %.2f", now / 1000, fps);
        lastMillis = now;
    }
}

/**
 * 这是一套基础配置的模板，配置串口，连接WiFi，启动OTA服务，准备日志服务器，并同步时间
 * 顺序和内容尽量不要变，以免启动即重启无法OTA重刷
 * 需要如下依赖：
   #include <WiFi.h>
   #include <esp_sntp.h>
   #include "httpota.h"
   #include "Logger.h"
   #include "ssid.h"
   OtaAssist ota(80);
   void timeCalibNotice(struct timeval *tv) { Serial.println("sntp time calibrated."); }
 */
void templateOfSetup()
{
    // 初始化串口
    Serial.begin(115200);
    Serial.println("Reboot");
    // 若是OTA升级，需要注释掉本条
    // ota.clearOtaData();
    ota.stableCheck();

    // Log初始化
    ExtLogger::instance().enableSerial();
    ExtLogger::instance().begin();
    XLOG("System", "ExtLogger initialized.");

    // 连接 WiFi
    XLOG("WiFi", "Connecting to WiFi...");
    WiFi.setHostname(HOSTNAME);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
        XLOG("WiFi", "Connection Failed! Rebooting...");
        delay(1000);
        ESP.restart();
    }
    XLOG("WiFi", "WIFI connected. IP = %s, Hostname = %s", WiFi.localIP().toString().c_str(), WiFi.getHostname());

    // LOG准备其他日志通道
    ExtLogger::instance().enableUDP(UDP_TARGET, UDP_PORT);
    // ExtLogger::instance().enableTelnet();

    // 启动OTA服务
    ota.initService();

    // 同步真实时间
    configTime(8 * 3600, 0, NTP_SERVER);
    sntp_set_time_sync_notification_cb(timeCalibNotice);
}

/**
 * 时间校准回调函数，与前面templateOfSetup()函数共用。
 */
void timeCalibNotice(struct timeval *tv)
{
    // 时间同步完成
    XLOG("System", "sntp time calibrated.");
}

void initPin()
{
    pinMode(39, OUTPUT);
    pinMode(40, OUTPUT);
    pinMode(41, OUTPUT);
    pinMode(42, OUTPUT);
    pinMode(9, OUTPUT);
    pinMode(10, OUTPUT);
    pinMode(14, OUTPUT);
    pinMode(21, OUTPUT);
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
    XLOG("System", "Reporting settings...");
    mqtt.sendDiscoveries();
}
