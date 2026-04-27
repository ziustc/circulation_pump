#include "mqtt.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ssid.h>
#include "extlogger.h"

// MQTT 主题常量
#define MQTT_START_TIME  "pump_st_"
#define MQTT_END_TIME    "pump_et_"
#define MQTT_WATER_MIN   "pump_wmin"
#define MQTT_WATER_MAX   "pump_wmax"
#define MQTT_DURATION    "pump_dur"
#define MQTT_DEMAND_TEMP "pump_demandt"
#define MQTT_PUMP_ON     "pump_on"

#define MQTT_STATE_WATER     "pump_s_wt"
#define MQTT_STATE_EXT_WATER "pump_s_ewt"
#define MQTT_STATE_FLOW      "pump_s_flow"
#define MQTT_STATE_ON        "pump_s_on"

// 静态成员变量定义
PumpMqttManager *PumpMqttManager::instance = nullptr;

// 构造函数
PumpMqttManager::PumpMqttManager()
: _client(_wifiClient)
{
    instance = this;
}

void PumpMqttManager::init()
{
    _client.setServer(MQTT_SERVER, MQTT_PORT);
    _client.setCallback(onReceive);
    _client.setBufferSize(1024);
    reconnect();
}

void PumpMqttManager::loop()
{
    if (!_client.connected()) reconnect();
    _client.loop();
}

void PumpMqttManager::setOnCmdCallback(OnCmdCallback_t cb) { _onMqttCmd_cb = cb; }

void PumpMqttManager::setOnStateCallback(OnStateCallback_t cb) { _onMqttState_cb = cb; }

void PumpMqttManager::setPumpOnCallback(OnPumpOnCallback_t cb) { _onMqttPumpOn_cb = cb; }

// 自动重连并订阅
void PumpMqttManager::reconnect()
{
    if (_client.connect(CLIENT_ID))
    {
        const char *subTopic = "hass/circ_pump/#";
        XLOG("MQTT", "connected, server = %s, port = %d.", MQTT_SERVER, MQTT_PORT);
        _client.subscribe(subTopic);
        XLOG("MQTT", "subscribed: %s", subTopic);
        int entityCount = sendDiscoveries();
        XLOG("MQTT", "%d entities registered.", entityCount);
    }
    else
        XLOG("MQTT", "connect failed, rc=%d.", _client.state());
}

void PumpMqttManager::sendMsg(const char *topic, const char *payload)
{
    if (_client.connected())
    {
        if (!_client.publish(topic, payload)) XLOG("MQTT", "send message failure! topic: %s", topic);
    }
}

int PumpMqttManager::sendDiscoveries()
{
    String topic;
    String payload;
    int    entityCount = 0;

    for (int i = 0; i < 3; i++)
    {
        topic   = String("homeassistant/text/") + MQTT_START_TIME + String(i + 1) + "/config";
        payload = timeDiscoveryPayload("保温起始时间" + String(i + 1), MQTT_START_TIME + String(i + 1));
        // XLOG("MQTT", "topic: %s\r\npayload: %s", topic.c_str(), payload.c_str());
        if (!_client.publish(topic.c_str(), payload.c_str(), true))
        {
            XLOG("MQTT", "%s failed to register", topic.c_str());
            entityCount++;
        }

        topic   = String("homeassistant/text/") + MQTT_END_TIME + String(i + 1) + "/config";
        payload = timeDiscoveryPayload("保温结束时间" + String(i + 1), MQTT_END_TIME + String(i + 1));
        // XLOG("MQTT", "topic: %s\r\npayload: %s", topic.c_str(), payload.c_str());
        if (!_client.publish(topic.c_str(), payload.c_str(), true))
        {
            XLOG("MQTT", "%s failed to register", topic.c_str());
            entityCount++;
        }
    }

    topic   = String("homeassistant/number/") + MQTT_WATER_MIN + "/config";
    payload = numberDiscoveryPayload("水控最短时间", MQTT_WATER_MIN, "秒", 1, 9);
    // XLOG("MQTT", "topic: %s\r\npayload: %s", topic.c_str(), payload.c_str());
    if (!_client.publish(topic.c_str(), payload.c_str(), true))
    {
        XLOG("MQTT", "%s failed to register", topic.c_str());
        entityCount++;
    }

    topic   = String("homeassistant/number/") + MQTT_WATER_MAX + "/config";
    payload = numberDiscoveryPayload("水控最长时间", MQTT_WATER_MAX, "秒", 1, 9);
    // XLOG("MQTT", "topic: %s\r\npayload: %s", topic.c_str(), payload.c_str());
    if (!_client.publish(topic.c_str(), payload.c_str(), true))
    {
        XLOG("MQTT", "%s failed to register", topic.c_str());
        entityCount++;
    }

    topic   = String("homeassistant/number/") + MQTT_DURATION + "/config";
    payload = numberDiscoveryPayload("每次开泵时长", MQTT_DURATION, "分", 1, 9);
    // XLOG("MQTT", "topic: %s\r\npayload: %s", topic.c_str(), payload.c_str());
    if (!_client.publish(topic.c_str(), payload.c_str(), true))
    {
        XLOG("MQTT", "%s failed to register", topic.c_str());
        entityCount++;
    }

    topic   = String("homeassistant/number/") + MQTT_DEMAND_TEMP + "/config";
    payload = numberDiscoveryPayload("保温温度", MQTT_DEMAND_TEMP, "°C", TEMP_SETTING_LOWEST, TEMP_SETTING_HIGHEST);
    // XLOG("MQTT", "topic: %s\r\npayload: %s", topic.c_str(), payload.c_str());
    if (!_client.publish(topic.c_str(), payload.c_str(), true))
    {
        XLOG("MQTT", "%s failed to register", topic.c_str());
        entityCount++;
    }

    topic   = String("homeassistant/button/") + MQTT_PUMP_ON + "/config";
    payload = buttonDiscoveryPayload("启动循环泵", MQTT_PUMP_ON);
    // XLOG("MQTT", "topic: %s\r\npayload: %s", topic.c_str(), payload.c_str());
    if (!_client.publish(topic.c_str(), payload.c_str(), true))
    {
        XLOG("MQTT", "%s failed to register", topic.c_str());
        entityCount++;
    }

    topic   = String("homeassistant/sensor/") + MQTT_STATE_WATER + "/config";
    payload = waterDiscoveryPayload("当前水温", MQTT_STATE_WATER);
    // XLOG("MQTT", "topic: %s\r\npayload: %s", topic.c_str(), payload.c_str());
    if (!_client.publish(topic.c_str(), payload.c_str(), true))
    {
        XLOG("MQTT", "%s failed to register", topic.c_str());
        entityCount++;
    }

    topic   = String("homeassistant/sensor/") + MQTT_STATE_FLOW + "/config";
    payload = flowDiscoveryPayload("流量", MQTT_STATE_FLOW);
    // XLOG("MQTT", "topic: %s\r\npayload: %s", topic.c_str(), payload.c_str());
    if (!_client.publish(topic.c_str(), payload.c_str(), true))
    {
        XLOG("MQTT", "%s failed to register", topic.c_str());
        entityCount++;
    }

    topic   = String("homeassistant/binary_sensor/") + MQTT_STATE_ON + "/config";
    payload = StateOnDiscoveryPayload("运行状态", MQTT_STATE_ON);
    // XLOG("MQTT", "topic: %s\r\npayload: %s", topic.c_str(), payload.c_str());
    if (!_client.publish(topic.c_str(), payload.c_str(), true))
    {
        XLOG("MQTT", "%s failed to register", topic.c_str());
        entityCount++;
    }
    return entityCount;
}

void PumpMqttManager::sendSettings(Settings_t &pumpSettings)
{
    _client.publish("hass/circ_pump/state", settingsPayload(pumpSettings).c_str());
}

void PumpMqttManager::sendState(State_t &pumpState)
{
    _client.publish("hass/circ_pump/state", statePayload(pumpState).c_str());
}

void PumpMqttManager::onReceive(const char *topic, const uint8_t *payload, unsigned int len)
{
    if (instance) instance->receiveMsg(topic, payload, len);
}

void PumpMqttManager::receiveMsg(const char *topic, const uint8_t *payload, unsigned int len)
{
    JsonDocument         doc;
    DeserializationError jsonError;

    // XLOG("MQTT.received.topic", topic);
    // XLOG("MQTT.received.payload", (char *)payload);

    jsonError = deserializeJson(doc, payload);
    if (jsonError) return;

    JsonObject jsonObj = doc.as<JsonObject>();
    if (strcmp(topic, "hass/circ_pump/cmd") == 0) handleCmd(jsonObj);
    if (strcmp(topic, "hass/circ_pump/state") == 0) handleState(jsonObj);
}

void PumpMqttManager::handleCmd(const JsonObject &obj)
{
    Settings_t set;

    // 如果收到开泵指令
    if (obj[MQTT_PUMP_ON].is<String>())
    {
        if (_onMqttPumpOn_cb) _onMqttPumpOn_cb();
        return;
    }

    // 如果收到设置参数，更新参数并回调输出（每次只处理一个参数）
    String startTimeKey;
    String endTimeKey;
    for (int i = 0; i < 3; i++)
    {
        startTimeKey = String(MQTT_START_TIME) + String(i + 1);
        endTimeKey   = String(MQTT_END_TIME) + String(i + 1);
        int h, m;

        if (obj[startTimeKey].is<String>())
        {
            const char *startTime = obj[startTimeKey];
            sscanf(startTime, "%d:%d", &h, &m);
            set.startHour[i]   = h;
            set.startMinute[i] = m;
            if (_onMqttCmd_cb) _onMqttCmd_cb(set);
            return;
        }

        if (obj[endTimeKey].is<String>())
        {
            const char *endTime = obj[endTimeKey];
            sscanf(endTime, "%d:%d", &h, &m);
            set.endHour[i]   = h;
            set.endMinute[i] = m;
            if (_onMqttCmd_cb) _onMqttCmd_cb(set);
            return;
        }
    }

    if (obj[MQTT_WATER_MIN].is<int>())
    {
        set.waterMinSec = obj[MQTT_WATER_MIN].as<int>();
        if (_onMqttCmd_cb) _onMqttCmd_cb(set);
        return;
    }
    if (obj[MQTT_WATER_MAX].is<int>())
    {
        set.waterMaxSec = obj[MQTT_WATER_MAX].as<int>();
        if (_onMqttCmd_cb) _onMqttCmd_cb(set);
        return;
    }
    if (obj[MQTT_DURATION].is<int>())
    {
        set.pumpOnDuration = obj[MQTT_DURATION].as<int>();
        if (_onMqttCmd_cb) _onMqttCmd_cb(set);
        return;
    }
    if (obj[MQTT_DEMAND_TEMP].is<int>())
    {
        set.demandTemp = obj[MQTT_DEMAND_TEMP].as<int>();
        if (_onMqttCmd_cb) _onMqttCmd_cb(set);
        return;
    }
}

void PumpMqttManager::handleState(const JsonObject &obj)
{
    State_t state;

    if (obj[MQTT_STATE_EXT_WATER].is<float>())
    {
        state.tempC2 = obj[MQTT_STATE_EXT_WATER].as<float>();
        if (_onMqttState_cb) _onMqttState_cb(state);
    }
}

String PumpMqttManager::timeDiscoveryPayload(String entityName, String entityId)
{
    JsonDocument doc = DiscoveryPayloadTemplate(entityName, entityId);
    doc["val_tpl"]   = String("{{ value_json.") + entityId + " }}";
    // doc["cmd_tpl"]   = String("{\"cmd\":\"") + entityId + "\",\"val\":\"{{ value }}\"}";
    doc["cmd_tpl"] = String("{\"") + entityId + "\": \"{{ value }}\"}";
    doc["pattern"] = "^([01]?[0-9]|2[0-3]):[0-5][0-9]$";
    doc["mode"]    = "text";
    doc["icon"]    = "mdi:clock-outline";

    String payloadStr;
    serializeJson(doc, payloadStr);
    return payloadStr;
}

String PumpMqttManager::numberDiscoveryPayload(
    String entityName, String entityId, const char *unit, int min, int max, int step)
{
    JsonDocument doc = DiscoveryPayloadTemplate(entityName, entityId);
    doc["val_tpl"]   = String("{{ value_json.") + entityId + " }}";
    // doc["cmd_tpl"]   = String("{\"cmd\":\"") + entityId + "\",\"val\":\"{{ value }}\"}";
    doc["cmd_tpl"]      = String("{\"") + entityId + "\": {{ value }} }";
    doc["min"]          = min;
    doc["max"]          = max;
    doc["step"]         = step;
    doc["unit_of_meas"] = unit;
    doc["mode"]         = "slider";

    String payloadStr;
    serializeJson(doc, payloadStr);
    return payloadStr;
}

String PumpMqttManager::buttonDiscoveryPayload(String entityName, String entityId)
{
    JsonDocument doc = DiscoveryPayloadTemplate(entityName, entityId);

    doc.remove("stat_t");
    // doc["cmd_tpl"] = String("{\"cmd\":\"") + entityId + "\",\"val\":\"ON\"}";
    doc["cmd_tpl"] = String("{\"") + entityId + "\": \"{{ value }}\"}";

    String payloadStr;
    serializeJson(doc, payloadStr);
    return payloadStr;
}

String PumpMqttManager::waterDiscoveryPayload(String entityName, String entityId)
{
    JsonDocument doc = DiscoveryPayloadTemplate(entityName, entityId);

    doc.remove("cmd_t");
    doc["stat_t"]       = "hass/circ_pump/state";
    doc["val_tpl"]      = String("{{ value_json.") + entityId + " }}";
    doc["unit_of_meas"] = "°C";
    doc["dev_cla"]      = "temperature";
    doc["stat_cla"]     = "measurement";

    String payloadStr;
    serializeJson(doc, payloadStr);
    return payloadStr;
}

String PumpMqttManager::flowDiscoveryPayload(String entityName, String entityId)
{
    JsonDocument doc = DiscoveryPayloadTemplate(entityName, entityId);

    doc.remove("cmd_t");
    doc["stat_t"]       = "hass/circ_pump/state";
    doc["val_tpl"]      = String("{{ value_json.") + entityId + " | float(1) }}";
    doc["unit_of_meas"] = "L/min";
    doc["ic"]           = "mdi:waves";
    doc["stat_cla"]     = "measurement";

    String payloadStr;
    serializeJson(doc, payloadStr);
    return payloadStr;
}

String PumpMqttManager::StateOnDiscoveryPayload(String entityName, String entityId)
{
    JsonDocument doc = DiscoveryPayloadTemplate(entityName, entityId);

    doc.remove("cmd_t");
    doc["stat_t"]  = "hass/circ_pump/state";
    doc["val_tpl"] = String("{{ \"ON\" if value_json.") + entityId + " else \"OFF\" }}";
    doc["dev_cla"] = "running";

    String payloadStr;
    serializeJson(doc, payloadStr);
    return payloadStr;
}

JsonDocument PumpMqttManager::DiscoveryPayloadTemplate(String entityName, String entityId)
{
    JsonDocument doc;
    doc["name"]      = entityName;
    doc["unique_id"] = entityId;
    doc["stat_t"]    = "hass/circ_pump/state";
    doc["cmd_t"]     = "hass/circ_pump/cmd";

    JsonObject dev        = doc["device"].to<JsonObject>();
    dev["identifiers"][0] = "circulation_pump";
    dev["name"]           = "循环泵";
    dev["manufacturer"]   = "ZiUtility";
    dev["model"]          = "ESP32-S3-Pump";
    dev["sw_version"]     = 0;
    dev["hw_version"]     = 0;
    dev["via_device"]     = "respi_gateway";

    return doc;
}

String PumpMqttManager::settingsPayload(Settings_t &pumpSettings)
{
    JsonDocument doc;
    char         buf[6];
    for (int i = 0; i < 3; i++)
    {
        snprintf(buf, sizeof(buf), "%02d:%02d", pumpSettings.startHour[i], pumpSettings.startMinute[i]);
        doc[MQTT_START_TIME + String(i + 1)] = buf;
        snprintf(buf, sizeof(buf), "%02d:%02d", pumpSettings.endHour[i], pumpSettings.endMinute[i]);
        doc[MQTT_END_TIME + String(i + 1)] = buf;
    }
    doc[MQTT_WATER_MIN]   = pumpSettings.waterMinSec; // 不带引号的数字字符
    doc[MQTT_WATER_MAX]   = pumpSettings.waterMaxSec;
    doc[MQTT_DURATION]    = pumpSettings.pumpOnDuration;
    doc[MQTT_DEMAND_TEMP] = pumpSettings.demandTemp;

    String payloadStr;
    serializeJson(doc, payloadStr);
    return payloadStr;
}

String PumpMqttManager::statePayload(State_t &pumpState)
{
    JsonDocument doc;

    doc[MQTT_STATE_WATER] = pumpState.tempC;
    doc[MQTT_STATE_FLOW]  = pumpState.flow;
    doc[MQTT_STATE_ON]    = pumpState.pumpOn;

    String payloadStr;
    serializeJson(doc, payloadStr);
    return payloadStr;
}
