#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
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

OtaAssist ota(80);

PumpMqttManager mqtt;
Screen          scr;
Button          btnUp, btnDown, btnOK, btnShift, btnPumpOn;
Button         *buttons[5] = {&btnUp, &btnDown, &btnOK, &btnShift, &btnPumpOn};
PumpCtrlUnit    ptu(scr, mqtt, PUMP_PIN, TEMP_PIN, FLOW_PIN);

void templateOfSetup();
void timeCalibNotice(struct timeval *tv);
void initPin();
void initButton();
void uploadSettings(Settings_t ctrlSet);

void setup(void)
{
    // 基础配置模板
    templateOfSetup();

    // 初始化引脚
    initPin();
    XLOG("Init", "Pins initialized.");

    // 准备显示屏
    scr.init();
    scr.setExportSettings_cb([](const Settings_t &set) { ptu.onScreenUpdate(set); });
    XLOG("Init", "Screen initialized.");

    // 按键初始化
    initButton();
    XLOG("Init", "Buttons initialized.");

    // 初始化MQTT
    mqtt.init();
    mqtt.setOnMsgCallback([](Settings_t set) { ptu.onMqttUpdate(set); });
    mqtt.setPumpOnCallback([]() { ptu.onMqttPumpOn(); });
    XLOG("Init", "MQTT connected.");

    // 泵控制单元初始化
    ptu.init();
    XLOG("Init", "Pump Control Unit initialized.");
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
    ptu.loop();

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
        XLOG("System", "running, uptime = %lus, FPS = %.2f", now / 1000, fps);
        lastMillis = now;
    }
}

/**
 * 这是一套基础配置的模板，配置串口，连接WiFi，启动OTA服务，准备日志服务器，并同步时间
 * 顺序和内容尽量不要变，以免启动即重启无法OTA重刷
 * 需要如下依赖：
    #include <Arduino.h>
    #include <WiFi.h>
    #include <esp_sntp.h>
    #include "httpota.h"
    #include "extlogger.h"
    #include "ssid.h"
 * 声明如下变量和函数
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
    XLOG("Init", "ExtLogger initialized.");

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
    XLOG("Init", "WIFI connected. IP = %s, Hostname = %s", WiFi.localIP().toString().c_str(), WiFi.getHostname());

    // LOG准备其他日志通道
    ExtLogger::instance().enableUDP(UDP_TARGET, UDP_PORT);
    // ExtLogger::instance().enableTelnet();

    // 启动OTA服务
    ota.initService();
    XLOG("Init", "HTTP OTA service initialized.");

    // 同步真实时间
    configTime(8 * 3600, 0, NTP_SERVER);
    sntp_set_time_sync_notification_cb(timeCalibNotice);
    XLOG("Init", "SNTP time sync initialized.");
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

    btnPumpOn.setTouchPin(TOUCH_START);
    btnPumpOn.setCallbackClick([]() { ptu.onButtonPumpOn(); });
}

/**
 * scr发生了数据更新，将新设置回传给外部
 */
void uploadSettings(Settings_t set)
{
    // 这里应使用MQTT将数据发送出去
    XLOG("System", "Reporting settings...");
}
