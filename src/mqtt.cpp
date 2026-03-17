#include "mqtt.h"
#include <Arduino.h> // 提供 String, sscanf, atoi 等
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

// 静态成员变量定义
PumpMqttManager *PumpMqttManager::instance = nullptr;

// 构造函数
PumpMqttManager::PumpMqttManager()
: _client(_wifiClient)
{
    instance = this;
}

void PumpMqttManager::begin()
{
    _client.setServer(MQTT_SERVER, MQTT_PORT);
    _client.setCallback(mqtt_received);
    _client.setBufferSize(1024);
    reconnect();
}

void PumpMqttManager::loop()
{
    if (!_client.connected()) reconnect();
    _client.loop();
}

void PumpMqttManager::setOnMsgCallback(OnMsgCallback_t cb) { _onMqttMsg_cb = cb; }

void PumpMqttManager::setPumpOnCallback(OnPumpOnCallback_t cb) { _onMqttPumpOn_cb = cb; }

// 自动重连并订阅
void PumpMqttManager::reconnect()
{
    static unsigned long lastMillis = -10000; // 保证第一次计算时长>5000，能启动连接
    String               clientId   = "CirculationPump";

    if (millis() - lastMillis >= 5000)
    {
        if (_client.connect(clientId.c_str()))
        {
            XLOG("MQTT", "connected, server = %s, port = %d.", MQTT_SERVER, MQTT_PORT);
            _client.subscribe("hass/#");
            XLOG("MQTT", "subscribe hass/#");
        }
        else
            XLOG("MQTT", "connect failed, rc=%d. retry in 5 seconds.", _client.state());
    }
}

void PumpMqttManager::sendMsg(const char *topic, const char *payload)
{
    if (_client.connected())
    {
        if (_client.publish(topic, payload))
        {
            XLOG("MQTT.topic", topic);
            XLOG("MQTT.payload", payload);
        }
        else
            XLOG("MQTT", "send failure");
    }
    return;
}

void PumpMqttManager::sendDiscoveries()
{
    String topic;
    String payload;

    for (int i = 0; i < 3; i++)
    {
        XLOG("MQTT.topic", topic.c_str());
        XLOG("MQTT.payload", payload.c_str());

        topic   = String("homeassistant/text/") + MQTT_START_TIME + String(i + 1) + "/config";
        payload = timeDiscoveryPayload("保温起始时间" + String(i + 1), MQTT_START_TIME + String(i + 1));
        if (!_client.publish(topic.c_str(), payload.c_str())) XLOG("MQTT", "%s failed to send", topic.c_str());

        topic   = String("homeassistant/text/") + MQTT_END_TIME + String(i + 1) + "/config";
        payload = timeDiscoveryPayload("保温结束时间" + String(i + 1), MQTT_END_TIME + String(i + 1));
        if (!_client.publish(topic.c_str(), payload.c_str())) XLOG("MQTT", "%s failed to send", topic.c_str());
    }

    topic   = String("homeassistant/number/") + MQTT_WATER_MIN + "/config";
    payload = numberDiscoveryPayload("水控最短时间", MQTT_WATER_MIN, "秒", 1, 9);
    if (!_client.publish(topic.c_str(), payload.c_str())) XLOG("MQTT", "%s failed to send", topic.c_str());

    topic   = String("homeassistant/number/") + MQTT_WATER_MAX + "/config";
    payload = numberDiscoveryPayload("水控最长时间", MQTT_WATER_MAX, "秒", 1, 9);
    if (!_client.publish(topic.c_str(), payload.c_str())) XLOG("MQTT", "%s failed to send", topic.c_str());

    topic   = String("homeassistant/number/") + MQTT_DURATION + "/config";
    payload = numberDiscoveryPayload("每次开泵时长", MQTT_DURATION, "分", 1, 9);
    if (!_client.publish(topic.c_str(), payload.c_str())) XLOG("MQTT", "%s failed to send", topic.c_str());

    topic   = String("homeassistant/number/") + MQTT_DEMAND_TEMP + "/config";
    payload = numberDiscoveryPayload("保温温度", MQTT_DEMAND_TEMP, "度", 25, 45);
    if (!_client.publish(topic.c_str(), payload.c_str())) XLOG("MQTT", "%s failed to send", topic.c_str());

    topic   = String("homeassistant/button/") + MQTT_PUMP_ON + "/config";
    payload = buttonDiscoveryPayload("启动循环泵", MQTT_PUMP_ON);
    if (_client.publish(topic.c_str(), payload.c_str())) XLOG("MQTT", topic.c_str());
}

void PumpMqttManager::sendState(Settings &pumpSettings)
{
    _client.publish("hass/pump/s", statePayload(pumpSettings).c_str());
}

void PumpMqttManager::mqtt_received(const char *topic, const uint8_t *payload, unsigned int len)
{
    if (instance) instance->handleCommand(topic, payload, len);
}

void PumpMqttManager::handleCommand(const char *topic, const uint8_t *payload, unsigned int len)
{
    JsonDocument         doc;
    DeserializationError error = deserializeJson(doc, payload);
    Settings             set;

    XLOG("MQTT.received.topic", topic);
    XLOG("MQTT.received.payload", (char *)payload);

    if (error) return;

    if (strcmp(topic, "hass/pump/c") != 0) return;

    const char *cmd = doc["cmd"];
    const char *val = doc["val"];

    // 如果直接收到开循环泵的指令
    if (strcmp(cmd, MQTT_PUMP_ON) == 0)
    {
        if (_onMqttPumpOn_cb) _onMqttPumpOn_cb();
        return;
    }

    // 如果收到参数
    if (strncmp(cmd, MQTT_START_TIME, sizeof(MQTT_START_TIME)) == 0)
    {
        int id;
        int h, m;
        sscanf(cmd, (String(MQTT_START_TIME) + "%d").c_str(), &id);
        sscanf(val, "%d:%d", &h, &m);

        set.startHour[id]   = h;
        set.startMinute[id] = m;
    }

    else if (strncmp(cmd, MQTT_END_TIME, sizeof(MQTT_END_TIME)) == 0)
    {
        int id;
        int h, m;
        sscanf(cmd, (String(MQTT_END_TIME) + "%d").c_str(), &id);
        sscanf(val, "%d:%d", &h, &m);

        set.endHour[id]   = h;
        set.endMinute[id] = m;
    }

    else if (strcmp(cmd, MQTT_WATER_MIN) == 0)
        set.waterMinSec = atoi(val);

    else if (strcmp(cmd, MQTT_WATER_MAX) == 0)
        set.waterMaxSec = atoi(val);

    else if (strcmp(cmd, MQTT_DURATION) == 0)
        set.pumpOnDuration = atoi(val);

    else if (strcmp(cmd, MQTT_DEMAND_TEMP) == 0)
        set.demandTemp = atoi(val);

    else
        return;

    // 回调外部函数输出参数
    if (_onMqttMsg_cb) _onMqttMsg_cb(set);
}

// String PumpMqttManager::timeDiscoveryPayload(String entityName, String entityId)
// {
//     JsonDocument doc = DiscoveryPayloadTemplate(entityName, entityId);
//     doc["val_tpl"]   = "{{ value_json." + entityId + " + ':00' }}";
//     doc["cmd_tpl"]   = "{\"cmd\":\"" + entityId + "\",\"val\":\"{{ value[:5] }}\"}";

//     String payloadStr;
//     serializeJson(doc, payloadStr);
//     return payloadStr;
// }

String PumpMqttManager::timeDiscoveryPayload(String entityName, String entityId)
{
    JsonDocument doc = DiscoveryPayloadTemplate(entityName, entityId);
    doc["pattern"]   = "^([01]?[0-9]|2[0-3]):[0-5][0-9]$";
    doc["mode"]      = "text";
    doc["icon"]      = "mdi:clock-outline";
    doc["val_tpl"]   = "{{ value_json." + entityId + " }}";
    doc["cmd_tpl"]   = "{\"cmd\":\"" + entityId + "\",\"val\":\"{{ value }}\"}";

    String payloadStr;
    serializeJson(doc, payloadStr);
    return payloadStr;
}

String PumpMqttManager::numberDiscoveryPayload(
    String entityName, String entityId, const char *unit, int min, int max, int step)
{
    JsonDocument doc    = DiscoveryPayloadTemplate(entityName, entityId);
    doc["val_tpl"]      = String("{{ value_json.") + entityId + " }}";
    doc["cmd_tpl"]      = String("{\"cmd\":\"") + entityId + "\",\"val\":\"{{value}}\"}";
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
    doc["cmd_tpl"] = String("{\"cmd\":\"") + entityId + "\",\"val\":\"ON\"}";

    String payloadStr;
    serializeJson(doc, payloadStr);
    return payloadStr;
}

JsonDocument PumpMqttManager::DiscoveryPayloadTemplate(String entityName, String entityId)
{
    JsonDocument doc;
    doc["name"]      = entityName;
    doc["unique_id"] = entityId;
    doc["stat_t"]    = "hass/pump/s";
    doc["cmd_t"]     = "hass/pump/c";

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

String PumpMqttManager::statePayload(Settings &pumpSettings)
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