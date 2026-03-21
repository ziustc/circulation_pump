#include "pcu.h"
#include "extlogger.h"
#include "common.h"
#include "extlogger.h"

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
    pulseIntervals[pulseIndex++] = (now - lastPulseTime);
    if (pulseIndex >= RECENT_DATA_COUNT) pulseIndex = 0;
    lastPulseTime = now;

    // 针对按频率计算
    pulseCount++;
}

/*************************************************************/
/*                       PumpCtrlUnit                        */
/*************************************************************/

TempSensor::TempSensor(int tempPin) { tempPinNo = tempPin; }

void TempSensor::init()
{
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
    flowSensor.init();
    tempSensor.init();

    // 确保初始状态为关闭
    switchPump(false);

    // 初始化设置参数，从NVS读取设置，若无则使用默认设置
    if (!readSettingsNVS())
    {
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
    }

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

    // 根据设置自动控制水泵
    if (!_state.pumpOn) tempCriteria();
    if (!_state.pumpOn) waterCriteria();
    if (_state.pumpOn) stopCriteria();
}

Settings_t PumpCtrlUnit::getSettings() { return _settings; }

State_t PumpCtrlUnit::getState() { return _state; }

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

    // 保存到NVS
    saveSettingsNVS();
}

void PumpCtrlUnit::onScreenUpdate(Settings_t set)
{
    _settings = set;
    _mqtt.sendSettings(_settings);
    saveSettingsNVS();
}

void PumpCtrlUnit::onMqttPumpOn()
{
    _pumpOnReason = PumpOnReason_t::MQTT;
    switchPump(true);
}

void PumpCtrlUnit::onButtonPumpOn()
{
    // 若当前未开泵，则切换为开泵，并设置开泵原因为按钮
    if (!_state.pumpOn)
    {
        _pumpOnReason = PumpOnReason_t::BUTTON;
        switchPump(true);
    }

    // 若当前已开泵，则切换为关泵，并重置开泵原因
    else
    {
        // 如果是温控开泵，按键关泵则视为特殊关泵，强制温控暂停一段时间，否则因为温度不到立即又开泵导致按键无法关断
        if (_pumpOnReason == PumpOnReason_t::TEMP_CRITERIA)
            _pumpOffReason = PumpOffReason_t::TEMP_BUTTON_OFF;
        else
            _pumpOffReason = PumpOffReason_t::NORMAL;
        _pumpOnReason = PumpOnReason_t::OFF;

        switchPump(false);
    }
}

void PumpCtrlUnit::saveSettingsNVS()
{
    prefs.begin("pump_settings", false);
    prefs.putBytes("settings", &_settings, sizeof(_settings));
    prefs.end();
}

bool PumpCtrlUnit::readSettingsNVS()
{
    bool ret = false;
    prefs.begin("pump_settings", true);
    if (prefs.isKey("settings"))
    {
        prefs.getBytes("settings", &_settings, sizeof(_settings));
        XLOG("PCU", "Settings loaded from NVS.");
        ret = true;
    }
    else
    {
        XLOG("PCU", "No settings found in NVS, using defaults.");
    }
    prefs.end();
    return ret;
}

void PumpCtrlUnit::switchPump(bool pumpOn)
{
    _state.pumpOn = pumpOn;
    if (pumpOn)
    {
        _pumpOnMillis = millis();
        digitalWrite(pumpPinNo, HIGH);
    }
    else
    {
        _pumpOnMillis  = 0;
        _pumpOffMillis = millis();
        digitalWrite(pumpPinNo, LOW);
    }

    _screen.updatePumpOn(pumpOn);
    _mqtt.sendState(_state);
    XLOG("PCU", "pump %s %s", pumpOn ? "ON" : "OFF", strOnOffComment().c_str());
}

void PumpCtrlUnit::readTemperature() { _state.tempC = tempSensor.getTempC(); }

void PumpCtrlUnit::readFlow() { _state.flow = flowSensor.getFlow(); }

void PumpCtrlUnit::readTime()
{
    time_t now = time(nullptr);
    localtime_r(&now, &realTime); // 线程安全版本
}

void PumpCtrlUnit::tempCriteria()
{
    // 先判断上次是否为超时强制关泵，或手动强制关泵，若是则判断是否已过恢复时间，已到则恢复正常状态
    if (_pumpOffReason == PumpOffReason_t::TEMP_OVERTIME || _pumpOffReason == PumpOffReason_t::TEMP_BUTTON_OFF)
    {
        if (millis() - _pumpOffMillis > TEMP_OVERTIME_RECOVERY * 60 * 1000)
        {
            XLOG("PCU",
                 "Pump lock ended. allow temp criteria again, last off reason: %s",
                 _pumpOffReason == PumpOffReason_t::TEMP_OVERTIME ? "overtime" : "button off");
            _pumpOffReason = PumpOffReason_t::NORMAL; // 恢复正常状态，允许再次开泵
        }
        // 未过则不允许开泵，直接返回
        else
        {
            return;
        }
    }

    // 判断当前是否在设定的保温时间段（针对每个时间段）
    bool inTimeRange = false;
    int  nowTotalMin = realTime.tm_hour * 60 + realTime.tm_min;

    for (int i = 0; i < 3; i++)
    {
        int startTotalMin = _settings.startHour[i] * 60 + _settings.startMinute[i];
        int endTotalMin   = _settings.endHour[i] * 60 + _settings.endMinute[i];

        // 跳过无效时间段
        if (startTotalMin == endTotalMin) continue;

        // 不跨夜的时间段
        if (startTotalMin <= endTotalMin)
        {
            if (nowTotalMin >= startTotalMin && nowTotalMin < endTotalMin)
            {
                inTimeRange = true;
                break;
            }
        }
        // 跨夜的时间段
        else
        {
            if (nowTotalMin >= startTotalMin || nowTotalMin < endTotalMin)
            {
                inTimeRange = true;
                break;
            }
        }
    }

    // 若在保温时间段内且温度未达设定值，则开启水泵
    if (inTimeRange && (_state.tempC < _settings.demandTemp - TEMP_LOWER_MARGIN) && !_state.pumpOn)
    {
        _pumpOnReason = PumpOnReason_t::TEMP_CRITERIA;
        switchPump(true);
    }
}

void PumpCtrlUnit::waterCriteria()
{
    // 避免上次关泵后水流未停止，先判断距上次关泵是否已超过最短间隔，若未超过则直接返回不判断水控条件
    if (millis() - _pumpOffMillis < WATER_MARGIN * 1000) return;

    // 判断_state.flow大于FLOW_THRESHOLD的时长，若时长处于_settings.waterMinSec和_settings.waterMaxSec之间，则开启水泵pumpOnDuration秒
    static unsigned long flowStartTime = 0;

    if (_state.flow > FLOW_THRESHOLD)
    {
        if (flowStartTime == 0) flowStartTime = millis();
    }

    else if (_state.flow < FLOW_THRESHOLD / 2)
    {
        if (flowStartTime != 0)
        {
            unsigned long flowDuration = (millis() - flowStartTime) / 1000; // 单位秒
            if (flowDuration >= _settings.waterMinSec && flowDuration <= _settings.waterMaxSec)
            {
                _pumpOnReason = PumpOnReason_t::WATER_CRITERIA;
                switchPump(true);
            }
            flowStartTime = 0;
        }
    }
}

void PumpCtrlUnit::stopCriteria()
{
    // 若没有开泵，直接返回
    if (!_state.pumpOn)
    {
        _pumpOnReason = PumpOnReason_t::OFF;
        return;
    }

    // 若按保温时段保温开泵，则按温度条件关泵
    if (_pumpOnReason == PumpOnReason_t::TEMP_CRITERIA)
    {
        if (_state.tempC >= _settings.demandTemp + TEMP_UPPER_MARGIN)
        {
            _pumpOnReason  = PumpOnReason_t::OFF;
            _pumpOffReason = PumpOffReason_t::NORMAL;
            switchPump(false);

            return;
        }
        if (millis() - _pumpOnMillis >= TEMP_OVERTIME_LIMIT * 60 * 1000) // 若温控开泵超过1小时，强制关泵以防过热
        {
            _pumpOnReason  = PumpOnReason_t::OFF;
            _pumpOffReason = PumpOffReason_t::TEMP_OVERTIME;
            switchPump(false);
            return;
        }
    }

    // 其余（水控开泵，按键开泵，MQTT开泵）均按持续时长关泵
    if (millis() - _pumpOnMillis >= _settings.pumpOnDuration * 60 * 1000)
    {
        _pumpOnReason  = PumpOnReason_t::OFF;
        _pumpOffReason = PumpOffReason_t::NORMAL;
        switchPump(false);
    }
}

String PumpCtrlUnit::strOnOffComment()
{
    switch (_pumpOnReason)
    {
    case PumpOnReason_t::TEMP_CRITERIA:
        return "by temperature";
    case PumpOnReason_t::WATER_CRITERIA:
        return "by water criteria";
    case PumpOnReason_t::BUTTON:
        return "by button";
    case PumpOnReason_t::MQTT:
        return "by MQTT";
    case PumpOnReason_t::OFF:
        switch (_pumpOffReason)
        {
        case PumpOffReason_t::TEMP_OVERTIME:
            return String("by heating overtime, temp criteria disabled for ") + TEMP_OVERTIME_RECOVERY + " min";
        case PumpOffReason_t::TEMP_BUTTON_OFF:
            return String("manually, temp criteria disabled for ") + TEMP_OVERTIME_RECOVERY + " min";
        default:
            return "";
        }
    default:
        return "reason unknown";
    }
}