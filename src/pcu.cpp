#include "pcu.h"
#include "common.h"

#define SENSOR_FREQ       500            // 500ms读取一次传感器
#define MQTT_STATE_FREQ   10 * 1000      // 10s发送一次状态到HASS
#define MQTT_SETTING_FREQ 10 * 60 * 1000 // 10分钟发送一次Discovery

/*************************************************************/
/*                        FlowSensor                         */
/*************************************************************/

FlowSensor *FlowSensor::instance = nullptr;

FlowSensor::FlowSensor(int pin)
{
    instance = this;
    pinNo    = pin;
    for (int i = 0; i < RECENT_DATA_COUNT; i++)
        pulseIntervals[i] = 0;

    attachInterruptArg(digitalPinToInterrupt(pin), isrHandler, this, RISING);
}

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
        flow            = frequency / K_FLOW;
        return;
    }

    // 否则，按最近n次脉冲在平均间隔计算
    unsigned int sum = 0;
    for (int i = 0; i < RECENT_DATA_COUNT; i++)
        sum += intervals[i];
    flow = 1000.0f / (sum / (float)RECENT_DATA_COUNT) / K_FLOW;
}

void IRAM_ATTR FlowSensor::handleInterrupt()
{
    unsigned long now = millis();
    if (now - lastPulseTime < ANTI_SHAKE) return; // 防抖

    // 针对按周期计算
    pulseIntervals[pulseIndex++] = (now - lastPulseTime);
    if (pulseIndex >= RECENT_DATA_COUNT) pulseIndex = 0;
    lastPulseTime = now;

    // 针对按频率计算
    pulseCount++;
}

/*************************************************************/
/*                       PumpCtrlUnit                        */
/*************************************************************/

TempSensor::TempSensor(int tempPin)
{
    tempPinNo = tempPin;
    analogReadResolution(12);
    analogSetPinAttenuation(tempPinNo, ADC_11db);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_12, ADC_WIDTH_BIT_12, 1100, &adcChars);
}

int TempSensor::getTempC()
{
    int sumADC = 0;
    for (int i = 0; i < ADC_ARR_LEN; i++)
        sumADC += adcArr[i];
    float        avgADC  = (float)sumADC / ADC_ARR_LEN;
    unsigned int voltage = esp_adc_cal_raw_to_voltage(avgADC, &adcChars);
    return (voltage / 1000.0f - B) / K;
}

void TempSensor::sensorTick()
{
    unsigned long now = millis();
    if (now - lastMillis >= FREQ_IN_MS)
    {
        adcArr[adcIndex++] = analogRead(tempPinNo);
        if (adcIndex >= ADC_ARR_LEN) adcIndex = 0;
    }
}

/*************************************************************/
/*                       PumpCtrlUnit                        */
/*************************************************************/

PumpCtrlUnit::PumpCtrlUnit(Screen &scr, PumpMqttManager &mqtt, int pumpPin, int tempPin, int flowPin)
: _screen(scr)
, _mqtt(mqtt)
, flowSensor(flowPin)
, tempSensor(tempPin)
, pumpPinNo(pumpPin)
, tempPinNo(tempPin)
, flowPinNo(flowPin)
{
}

void PumpCtrlUnit::init()
{
    // 确保初始状态为关闭
    switchPump(false);

    // 初始化设置参数
    for (int i = 0; i < 3; i++)
    {
        _settings.startHour[i]   = 0;
        _settings.startMinute[i] = 0;
        _settings.endHour[i]     = 0;
        _settings.endMinute[i]   = 0;
    }
    _settings.waterMinSec    = 2;
    _settings.waterMaxSec    = 6;
    _settings.pumpOnDuration = 4;
    _settings.demandTemp     = 35;

    _screen.importSettings(_settings);

    // 发送一次discovery给HASS以创建实体，并发送初始化设置
    _mqtt.sendDiscoveries();
    _mqtt.sendSettings(_settings);
}

void PumpCtrlUnit::loop()
{
    unsigned long now = millis();

    tempSensor.sensorTick();

    // 每500ms读取一次传感器和时间，并刷新显示屏
    if (now - lastMillis_sensor > SENSOR_FREQ)
    {
        lastMillis_sensor = now;
        readTemperature();
        readFlow();
        readTime();

        // 刷新显示屏示数
        _screen.updateTempC(_state.tempC);
        _screen.updateFlow(_state.flow);
        _screen.updateTime(realTime.tm_hour, realTime.tm_min, realTime.tm_sec);
    }

    // 每10秒发送一次状态到HASS
    if (now - lastMillis_mqtt > MQTT_STATE_FREQ)
    {
        lastMillis_mqtt = now;
        _mqtt.sendSettings(_settings);
        _mqtt.sendState(_state);
    }

    // 每10分钟发送一次discovery以防HASS重启丢失实体
    if (now - lastMillis_discovery > MQTT_SETTING_FREQ)
    {
        lastMillis_discovery = now;
        _mqtt.sendDiscoveries();
    }
}

Settings_t PumpCtrlUnit::exportSettings()
{
    _settings = _screen.exportSettings();
    return _settings;
}

void PumpCtrlUnit::onMqttUpdate(Settings_t set)
{
    // 更新设置参数
    for (int i = 0; i < 3; i++)
    {
        if (set.startHour[i] >= 0) _settings.startHour[i] = set.startHour[i];
        if (set.startMinute[i] >= 0) _settings.startMinute[i] = set.startMinute[i];
        if (set.endHour[i] >= 0) _settings.endHour[i] = set.endHour[i];
        if (set.endMinute[i] >= 0) _settings.endMinute[i] = set.endMinute[i];
    }
    if (set.waterMinSec >= 0) _settings.waterMinSec = set.waterMinSec;
    if (set.waterMaxSec >= 0) _settings.waterMaxSec = set.waterMaxSec;
    if (set.pumpOnDuration >= 0) _settings.pumpOnDuration = set.pumpOnDuration;
    if (set.demandTemp >= 0) _settings.demandTemp = set.demandTemp;

    // 更新屏幕显示并反馈确认（有可能因为输入超出范围而被修正）
    _screen.importSettings(_settings);
    _settings = _screen.exportSettings();

    // 回传HASS确认
    _mqtt.sendSettings(_settings);
}

void PumpCtrlUnit::onScreenUpdate(Settings_t set)
{
    _settings = set;
    _mqtt.sendSettings(_settings);
}

void PumpCtrlUnit::onMqttPumpOn() { switchPump(true); }

void PumpCtrlUnit::onButtonPumpOn() { switchPump(!_state.pumpOn); }

void PumpCtrlUnit::switchPump(bool pumpOn)
{
    _state.pumpOn = pumpOn;
    _screen.updatePumpOn(pumpOn);
    _mqtt.sendState(_state);
}

void PumpCtrlUnit::readTemperature() { _state.tempC = tempSensor.getTempC(); }

void PumpCtrlUnit::readFlow() { _state.flow = flowSensor.getFlow(); }

void PumpCtrlUnit::readTime()
{
    time_t now = time(nullptr);
    localtime_r(&now, &realTime); // 线程安全版本
}