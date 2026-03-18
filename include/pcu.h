#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <esp_adc_cal.h>
#include <deque>
#include <screen.h>
#include "mqtt.h"

using namespace std;

class FlowSensor
{
public:
    FlowSensor(int pin);
    /**
     * 读取流量值。读取时计算，为保证计算准确性，最好是约0.5~1秒读一次
     */
    float getFlow();

private:
    static constexpr int   RECENT_DATA_COUNT = 5;     // 滚动记录最近5次脉冲周期
    static constexpr int   ANTI_SHAKE        = 3;     // 单位：毫秒。防抖阈值
    static constexpr int   FREQ_VS_CYCLE     = 10;    // 单位：Hz。高于10Hz按脉冲次数算；低于按周期平均值算
    static constexpr float K_FLOW            = 500.0; // 流量常数，每L多少脉冲

    static FlowSensor *instance;

    int   pinNo;
    float flow = 0;

    // ===== ISR共享数据 =====
    volatile unsigned long lastPulseTime = 0;
    volatile unsigned int  pulseIntervals[RECENT_DATA_COUNT];
    volatile int           pulseIndex = 0;
    volatile int           pulseCount = 0;

    unsigned long lastCalcTime = 0;

    void                  calcFlow();
    void IRAM_ATTR        handleInterrupt();
    static void IRAM_ATTR isrHandler(void *arg)
    {
        FlowSensor *self = static_cast<FlowSensor *>(arg);
        self->handleInterrupt();
    }
};

class TempSensor
{
public:
    TempSensor(int tempPin);
    int  getTempC();
    void sensorTick();

private:
    static constexpr int          ADC_ARR_LEN = 16;     // 温度采样32次平均值
    static constexpr int          FREQ_IN_MS  = 10;     // 采样频率，10ms一次，实际因其他阻塞函数，本身要近20ms才刷一次
    static constexpr float        K           = 0.0269; // ADC = K * tempC + B，倒推计算tempC
    static constexpr float        B           = 0.45;   // ADC = K * tempC + B，倒推计算tempC
    int                           adcArr[ADC_ARR_LEN] = {0}; // 温度采样历史数据（用于取平均值）
    int                           adcIndex            = 0;
    int                           tempPinNo;
    esp_adc_cal_characteristics_t adcChars;
    unsigned long                 lastMillis = 0;
};

class PumpCtrlUnit
{
public:
    PumpCtrlUnit(Screen &scr, PumpMqttManager &mqtt, int pumpPin, int tempPin, int flowPin);
    void       init();
    void       loop();
    Settings_t exportSettings();
    void       onMqttUpdate(Settings_t set);
    void       onScreenUpdate(Settings_t set);
    void       onMqttPumpOn();
    void       onButtonPumpOn();

private:
    Screen          &_screen;    // 显示屏对象
    PumpMqttManager &_mqtt;      // MQTT对象
    FlowSensor       flowSensor; // 流量传感器对象
    TempSensor       tempSensor; // 温度传感器对象
    int              pumpPinNo;  // 泵引脚号
    int              tempPinNo;  // 温度引脚号
    int              flowPinNo;  // 流速引脚号

    Settings_t    _settings; // 当前循环泵的设置
    State_t       _state;    // 当前循环泵的状态（含传感器）
    int           tempC  = 25;
    float         flow   = 0;
    bool          pumpOn = false;
    struct tm     realTime;
    unsigned long lastMillis_sensor    = 0;
    unsigned long lastMillis_mqtt      = 0;
    unsigned long lastMillis_discovery = 0;
    void          switchPump(bool pumpOn);
    void          readTemperature();
    void          readFlow();
    void          readTime();
};
