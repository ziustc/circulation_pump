#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <time.h>
#include <deque>
#include <screen.h>
#include "mqtt.h"
#include "extlogger.h"
#include "sensor.h"

using namespace std;

class PumpCtrlUnit
{
public:
    PumpCtrlUnit(Screen &scr, PumpMqttManager &mqtt, int pumpPin, int tempPin, int flowPin);
    void       init();
    void       loop();
    Settings_t getSettings();
    State_t    getState();

    void onMqttUpdate(Settings_t set);
    void onMqttUpdate(State_t state);
    void onScreenUpdate(Settings_t set);
    void onMqttPumpOn();
    void onButtonPumpOn();

private:
    enum class PumpOnReason_t
    {
        TEMP_CRITERIA,
        WATER_CRITERIA,
        BUTTON,
        MQTT,
        OFF
    };

    enum class PumpOffReason_t
    {
        NORMAL,         // 正常关泵，温控达到指定温度，水控达到指定时长，BUTTON和MQTT开泵达到指定时长
        TEMP_OVERTIME,  // 特殊关泵，温控开启时长太久
        TEMP_BUTTON_OFF // 特殊关泵，温控开泵时按键关闭
    };

    Preferences      prefs;      // NVS对象
    Screen          &_screen;    // 显示屏对象
    PumpMqttManager &_mqtt;      // MQTT对象
    FlowSensor       flowSensor; // 流量传感器对象
    TempSensor       tempSensor; // 温度传感器对象
    int              pumpPinNo;  // 泵引脚号
    int              tempPinNo;  // 温度引脚号
    int              flowPinNo;  // 流速引脚号

    Settings_t      _settings;                                // 当前循环泵的设置
    State_t         _state;                                   // 当前循环泵的状态（含传感器）
    PumpOnReason_t  _pumpOnReason  = PumpOnReason_t::OFF;     // 水泵开启原因
    PumpOffReason_t _pumpOffReason = PumpOffReason_t::NORMAL; // 水泵关闭原因
    unsigned long   _pumpOnMillis  = 0;
    unsigned long   _pumpOffMillis = 0;
    // int             tempC          = 25; // 泵体温度
    // int             tempC2         = 25; // 用水端温度
    // float           flow           = 0;
    // bool            pumpOn         = false;
    struct tm     realTime;
    unsigned long lastMillis_sensor    = 0;
    unsigned long lastMillis_mqtt      = 0;
    unsigned long lastMillis_discovery = 0;
    void          saveSettingsNVS();
    bool          readSettingsNVS();
    void          switchPump(bool pumpOn);
    void          readTemperature();
    void          readFlow();
    void          readTime();
    void          tempCriteria();
    void          waterCriteria();
    void          stopCriteria();
    String        strOnOffComment();
    String        strPumpOffReason();
};
