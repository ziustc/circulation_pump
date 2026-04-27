#include <Arduino.h>
#include "common.h"

class FlowSensor
{
public:
    FlowSensor(int pin);
    /**
     * 读取流量值。读取时计算，为保证计算准确性，最好是约0.5~1秒读一次
     */
    float getFlow();
    void  init();

private:
    static constexpr int   RECENT_DATA_COUNT = 5;   // 滚动记录最近5次脉冲周期
    static constexpr int   ANTI_SHAKE        = 3;   // 单位：毫秒。防抖阈值
    static constexpr int   FREQ_VS_CYCLE     = 10;  // 单位：Hz。高于10Hz按脉冲次数算；低于按周期平均值算
    static constexpr float K_FLOW            = 650; // 单位：pulse/L。流量常数，每L多少脉冲

    int   pinNo;
    float flow = 0;

    // ===== ISR共享数据 =====
    volatile unsigned long lastPulseTime = 0;
    volatile unsigned int  pulseIntervals[RECENT_DATA_COUNT];
    volatile int           pulseIndex = 0;
    volatile int           pulseCount = 0;

    unsigned long lastCalcTime = 0;

    void        calcFlow();
    void        handleInterrupt();
    static void isrHandler(void *arg);
};

class TempSensor
{
public:
    TempSensor(int tempPin);
    void init();
    int  getTempC();
    void loop();

private:
    static constexpr int   ADC_ARR_LEN         = 16;     // 温度采样32次平均值
    static constexpr int   FREQ_IN_MS          = 10;     // 采样频率，10ms一次，实际因其他阻塞函数，本身要近20ms才刷一次
    static constexpr float K                   = 0.0269; // ADC = K * tempC + B，倒推计算tempC
    static constexpr float B                   = 0.45;   // ADC = K * tempC + B，倒推计算tempC
    int                    adcArr[ADC_ARR_LEN] = {0};    // 温度采样历史数据（用于取平均值）
    int                    adcIndex            = 0;
    int                    tempPinNo;
    unsigned long          lastMillis = 0;
};
