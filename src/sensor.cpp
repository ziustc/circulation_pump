#include "sensor.h"

/*************************************************************/
/*                        FlowSensor                         */
/*************************************************************/

FlowSensor::FlowSensor(int pin)
{
    pinNo = pin;
    for (int i = 0; i < RECENT_DATA_COUNT; i++)
        pulseIntervals[i] = 0;
}

void FlowSensor::init() { attachInterruptArg(digitalPinToInterrupt(pinNo), isrHandler, this, RISING); }

float FlowSensor::getFlow()
{
    // 如果间隔太短，则不计算直接返回上次的结果
    if (millis() - lastCalcTime < 200) return flow;

    // 计算本次间隔的流量
    calcFlow();
    return flow;
}

void FlowSensor::calcFlow()
{
    unsigned long now      = millis();
    unsigned long lastCalc = lastCalcTime;

    // ====== 数据快照 ======
    /* 由于以下数据会在中断中被修改，需要先关中断并快速保存当前数据（快照）
     * 用于接下来的计算，然后开中断以便于计算过程中及时响应新数据
     * 计算时使用快照，不会被中断意外修改，新的中断修改的数据也会得到记录用于下次计算
     * */
    noInterrupts();

    unsigned long lastPulse = lastPulseTime;
    int           count     = pulseCount;
    pulseCount              = 0;

    unsigned int intervals[RECENT_DATA_COUNT];
    for (int i = 0; i < RECENT_DATA_COUNT; i++)
        intervals[i] = pulseIntervals[i];

    lastCalcTime = millis();
    interrupts();
    // ====== 快照结束 ======

    // 若超过1秒无脉冲，视为0流量
    if (now - lastPulse > 1000)
    {
        flow = 0;
        return;
    }

    // 若频率大于阈值，按周期内脉冲次数算
    float deltaTime     = (now - lastCalc) / 1000.0f;
    float freqThreshold = deltaTime * FREQ_VS_CYCLE;
    if (count > freqThreshold)
    {
        float frequency = count / deltaTime;
        flow            = frequency * 60 / K_FLOW;
        // XLOG("FLOW", "by freq, flow = %.2f, count=%d, delta=%.2f", flow, count, deltaTime);
        return;
    }

    // 否则，按最近n次脉冲在平均间隔计算
    unsigned int sum = 0;
    for (int i = 0; i < RECENT_DATA_COUNT; i++)
        sum += intervals[i];
    flow = 1000.0f / ((float)sum / RECENT_DATA_COUNT) * 60 / K_FLOW;
    // XLOG("FLOW", "by interval, flow = %.2f, intervals=%.2f", flow, (float)sum / RECENT_DATA_COUNT);
}

void IRAM_ATTR FlowSensor::isrHandler(void *arg)
{
    FlowSensor *self = static_cast<FlowSensor *>(arg);
    self->handleInterrupt();
}

void IRAM_ATTR FlowSensor::handleInterrupt()
{
    unsigned long now = millis();
    if (now - lastPulseTime < ANTI_SHAKE) return; // 防抖

    // 针对按周期计算
    pulseIntervals[pulseIndex] = (now - lastPulseTime);
    pulseIndex                 = (pulseIndex + 1) % RECENT_DATA_COUNT;
    lastPulseTime              = now;

    // 针对按频率计算
    pulseCount = pulseCount + 1;
}

/*************************************************************/
/*                        TempSensor                         */
/*************************************************************/

TempSensor::TempSensor(int tempPin) { tempPinNo = tempPin; }

void TempSensor::init()
{
    analogReadResolution(12);
    analogSetPinAttenuation(tempPinNo, ADC_11db);
}

int TempSensor::getTempC()
{
    int sumADC = 0;
    for (int i = 0; i < ADC_ARR_LEN; i++)
        sumADC += adcArr[i];
    float avgADC = (float)sumADC / ADC_ARR_LEN;

    return (avgADC / 1000.0f - B) / K;
}

void TempSensor::loop()
{
    unsigned long now = millis();
    if (now - lastMillis >= FREQ_IN_MS)
    {
        adcArr[adcIndex] = analogReadMilliVolts(tempPinNo); // 直接读取mV值以减少计算误差
        adcIndex         = (adcIndex + 1) % ADC_ARR_LEN;
        lastMillis       = now;
    }
}