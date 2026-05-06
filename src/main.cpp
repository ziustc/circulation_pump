#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include "httpota.h"
#include "extlogger.h"
#include "mqtt2.h"
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
#define BEEP_PIN    15

using namespace std;

long lastMillis = 0;

OtaAssist    ota(80);
MqttManager  mqtt;
Screen       scr;
Button       btnUp, btnDown, btnOK, btnShift, btnPumpOn;
Button      *buttons[5] = {&btnUp, &btnDown, &btnOK, &btnShift, &btnPumpOn};
PumpCtrlUnit ptu(scr, mqtt, PUMP_PIN, TEMP_PIN, FLOW_PIN);

void timeCalibNotice(struct timeval *tv);
void initPin();
void initButton();

void setup(void)
{
    // 初始化串口
    Serial.begin(115200);
    Serial.println("=====Reboot================Reboot================Reboot=====");

    // 检查OTA版本状态，若不稳定则回滚到上一个稳定版本
    // ota.clearOtaData(); // 若是OTA升级，需要注释掉本条
    ota.stableCheck();

    // 初始化引脚
    initPin();

    // Log初始化
    ExtLogger::instance().init("circ_pump", false);
    ExtLogger::instance().enableSerial();
    XLOG("Init", "ExtLogger initialized.");

    // 连接 WiFi
    XLOG("WiFi", "Connecting to WiFi...");
    WiFi.setHostname(HOSTNAME);
    WiFi.mode(WIFI_STA);

    bool wifiConnected = false;
    for (int i = 0; i < 5; i++)
    {
        WiFi.begin(SSID, PASSWORD);
        if (WiFi.waitForConnectResult() == WL_CONNECTED)
        {
            wifiConnected = true;
            break;
        }
        XLOG("WiFi", "Connection Failed! Retrying...");
        delay(500);
    }
    if (!wifiConnected)
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

    // 初始化引脚
    XLOG("Init", "Pins initialized.");

    // 测试
    XLOG("TEST", "This is a test log message.");

    // 准备显示屏
    scr.init();
    scr.setExportSettings_cb([](const Settings_t &set) { ptu.onScreenUpdate(set); });
    XLOG("Init", "Screen initialized.");

    // 按键初始化
    initButton();
    XLOG("Init", "Buttons initialized.");

    // 初始化MQTT
    mqtt.init();
    mqtt.setOnCmdCB([](Settings_t set) { ptu.onMqttUpdate(set); });
    mqtt.setOnStateCB([](State_t state) { ptu.onMqttUpdate(state); });
    mqtt.setOnSwitchCB([](bool setOn) { ptu.onMqttPumpOn(setOn); });
    XLOG("Init", "MQTT initialized.");

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
        State_t state = ptu.getState();
        XLOG("PCU",
             "State: tempC=%d C, tempC2=%d C, flow=%.1f L/min, pumpOn=%s",
             state.tempC,
             state.tempC2,
             state.flow,
             state.pumpOn ? "ON" : "OFF");
        lastMillis = now;
    }
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
    // 按键，传感器，泵，蜂鸣器引脚
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, LOW);

    pinMode(BEEP_PIN, OUTPUT);
    digitalWrite(BEEP_PIN, LOW);

    pinMode(FLOW_PIN, INPUT);
    pinMode(TEMP_PIN, INPUT);

    pinMode(TOUCH_UP, INPUT_PULLUP);
    pinMode(TOUCH_DOWN, INPUT_PULLUP);
    pinMode(TOUCH_SHIFT, INPUT_PULLUP);
    pinMode(TOUCH_OK, INPUT_PULLUP);
    pinMode(TOUCH_START, INPUT_PULLUP);
}

void initButton()
{
    btnUp.setTouchPin(TOUCH_UP);
    btnUp.setBeepPin(BEEP_PIN);
    btnUp.setLongPressEnable(true);
    btnUp.setClickCallback([]() { scr.onUp(); });

    btnDown.setTouchPin(TOUCH_DOWN);
    btnDown.setBeepPin(BEEP_PIN);
    btnDown.setLongPressEnable(true);
    btnDown.setClickCallback([]() { scr.onDown(); });

    btnOK.setTouchPin(TOUCH_OK);
    btnOK.setBeepPin(BEEP_PIN);
    btnOK.setClickCallback([]() { scr.onOK(); });

    btnShift.setTouchPin(TOUCH_SHIFT);
    btnShift.setBeepPin(BEEP_PIN);
    btnShift.setClickCallback([]() { scr.onShift(); });

    btnPumpOn.setTouchPin(TOUCH_START);
    btnPumpOn.setBeepPin(BEEP_PIN);
    btnPumpOn.setClickCallback([]() { ptu.onButtonPumpOn(); });
    btnPumpOn.setVeryLongPressEnable(true);
    btnPumpOn.setVeryLongPressCallback([]() { ESP.restart(); });
}
