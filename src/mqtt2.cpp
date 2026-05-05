#include "mqtt2.h"

MqttCore    *MqttCore::coreInstance       = nullptr;
MqttManager *MqttManager::managerInstance = nullptr;

//========================================================//
//                      MqttCore                          //
//========================================================//

MqttCore::MqttCore() { coreInstance = this; }

void MqttCore::init(MqttInitParams_t mqttInfo, DeviceInfo_t devInfo, OnMqttReceive_t cb)
{
    _mqttInfo    = mqttInfo;
    _devInfo     = devInfo;
    onReceive_cb = cb;

    _client.setClient(_wifiClient);
    _client.setServer(mqttInfo.server, mqttInfo.port);
    _client.setCallback(onReceive);
    _client.setBufferSize(mqttInfo.bufSize);

    reconnect();

    // 预处理生成状态和命令topic字符串
    snprintf(_stateJson, sizeof(_stateJson), "hass/%s/state", _devInfo.id_abbr);
    snprintf(_cmdJson, sizeof(_cmdJson), "hass/%s/cmd", _devInfo.id_abbr);
}

void MqttCore::loop()
{
    if (!_client.connected()) reconnect();
    _client.loop();
}

void MqttCore::reconnect()
{
    char subTopic[20];

    if (_client.connect(_mqttInfo.clientId))
    {
        snprintf(subTopic, sizeof(subTopic), "hass/%s/#", _devInfo.id_abbr);
        _client.subscribe(subTopic);
    }
    else
    {
        XLOG("MQTT", "connect failed, rc=%d.", _client.state());
    }
}

void MqttCore::onReceive(const char *topic, const uint8_t *payload, unsigned int len)
{
    if (coreInstance && coreInstance->onReceive_cb) coreInstance->onReceive_cb(topic, payload, len);
}

bool MqttCore::sendDiscoveryBase(const char         *type,
                                 const char         *entityName,
                                 const char         *entityId,
                                 const char         *icon,
                                 const JsonDocument &attr,
                                 bool                fullDevInfo)
{
    JsonDocument doc;
    JsonObject   dev;
    char         topic[100];
    char         payload[1024];
    char         buf[50];

    // topic格式：homeassistant/{entityType}/{uniqueId}/config
    snprintf(topic, sizeof(topic), "homeassistant/%s/%s/config", type, entityId);

    // payload
    // ----固定属性
    doc["name"]      = entityName;
    doc["unique_id"] = entityId;
    if (icon && icon[0]) doc["icon"] = icon;
    doc["stat_t"] = _stateJson;
    doc["cmd_t"]  = _cmdJson;
    snprintf(buf, sizeof(buf), "{{ value_json.%s }}", entityId);
    doc["val_tpl"] = buf;
    snprintf(buf, sizeof(buf), "{\"%s\": {{ value }} }", entityId);
    doc["cmd_tpl"] = buf;

    // ----attr中的专用属性
    for (JsonPairConst kv : attr.as<JsonObjectConst>())
        doc[kv.key()] = kv.value();

    // ----设备信息（如果之前已经发送过完整dev属性，后续的就可以只发送identifier以节省流量）
    if (fullDevInfo)
    {
        dev                   = doc["device"].to<JsonObject>();
        dev["identifiers"][0] = _devInfo.identifier;
        dev["name"]           = _devInfo.name;
        dev["manufacturer"]   = _devInfo.manufacturer;
        dev["model"]          = _devInfo.model;
        dev["sw_version"]     = _devInfo.swVersion;
        dev["hw_version"]     = _devInfo.hwVersion;
        dev["via_device"]     = _devInfo.viaDevice;
    }
    else
        doc["dev"]["identifiers"][0] = _devInfo.identifier;

    // 发送
    serializeJson(doc, payload, sizeof(payload));
    if (_client.publish(topic, payload, true)) return true;
    return false;
}

bool MqttCore::sendDiscoveryNumber(const char *entityName,
                                   const char *entityId,
                                   const char *icon,
                                   const char *unit,
                                   int         min,
                                   int         max,
                                   int         step,
                                   bool        fullDevInfo)
{
    JsonDocument attr;
    if (unit && unit[0]) attr["unit"] = unit;
    attr["min"]  = min;
    attr["max"]  = max;
    attr["step"] = step;

    return sendDiscoveryBase("number", entityName, entityId, icon, attr, fullDevInfo);
}

bool MqttCore::sendDiscoverySwitch(const char *entityName, const char *entityId, const char *icon, bool fullDevInfo)
{
    JsonDocument attr;
    char         cmd_tpl[50];

    attr["pl_on"]  = "ON";
    attr["pl_off"] = "OFF";
    snprintf(cmd_tpl, sizeof(cmd_tpl), "{\"%s\": \"{{ value }}\"}", entityId);
    attr["cmd_tpl"] = cmd_tpl;

    return sendDiscoveryBase("switch", entityName, entityId, icon, attr, fullDevInfo);
}

bool MqttCore::sendDiscoveryTime(const char *entityName,
                                 const char *entityId,
                                 const char *icon,
                                 bool        has_date,
                                 bool        has_time,
                                 const char *val_tpl,
                                 const char *cmd_tpl,
                                 bool        fullDevInfo)
{
    JsonDocument attr;
    attr["has_date"] = has_date;
    attr["has_time"] = has_time;
    if (val_tpl && val_tpl[0]) attr["val_tpl"] = val_tpl;
    if (cmd_tpl && cmd_tpl[0]) attr["cmd_tpl"] = cmd_tpl;

    return sendDiscoveryBase("time", entityName, entityId, icon, attr, fullDevInfo);
}

bool MqttCore::sendDiscoverySensor(const char *entityName,
                                   const char *entityId,
                                   const char *icon,
                                   const char *stat_cla,
                                   const char *unit_of_meas,
                                   bool        fullDevInfo)
{
    JsonDocument attr;
    if (stat_cla && stat_cla[0]) attr["stat_cla"] = stat_cla;
    if (unit_of_meas && unit_of_meas[0]) attr["unit_of_meas"] = unit_of_meas;

    return sendDiscoveryBase("sensor", entityName, entityId, icon, attr, fullDevInfo);
}

bool MqttCore::sendState(const JsonDocument &states)
{
    JsonDocument doc;
    char         topic[100];
    char         payload[1024];

    for (JsonPairConst kv : states.as<JsonObjectConst>())
        doc[kv.key()] = kv.value();

    snprintf(topic, sizeof(topic), "hass/%s/state", _devInfo.id_abbr);
    serializeJson(doc, payload, sizeof(payload));

    if (_client.publish(topic, payload, false)) return true;
    return false;
}

//========================================================//
//                      MqttManager                       //
//========================================================//

#define MQTT_DEV_ABBR "circ_pump"

#define MQTT_START_TIME  "pump_st_"
#define MQTT_END_TIME    "pump_et_"
#define MQTT_WATER_MIN   "pump_wmin"
#define MQTT_WATER_MAX   "pump_wmax"
#define MQTT_DURATION    "pump_dur"
#define MQTT_DEMAND_TEMP "pump_demandt"

#define MQTT_STATE_WATER     "pump_s_wt"
#define MQTT_STATE_EXT_WATER "pump_s_ewt"
#define MQTT_STATE_FLOW      "pump_s_flow"
#define MQTT_PUMP_ON         "pump_s_on"

MqttManager::MqttManager() { managerInstance = this; }

void MqttManager::init()
{
    MqttInitParams_t mqttInfo = {.clientId = HOSTNAME, .server = "192.168.0.20", .port = 1883, .bufSize = 1024};

    DeviceInfo_t devInfo = {.identifier   = "circulation_pump",
                            .name         = "循环泵",
                            .id_abbr      = MQTT_DEV_ABBR,
                            .manufacturer = "ZiUtility",
                            .model        = "ESP32-S3-Pump",
                            .viaDevice    = "respi_gateway",
                            .swVersion    = "1.0.0",
                            .hwVersion    = "1.0.0"};

    _mqttCore.init(mqttInfo, devInfo, onReceive);
}

void MqttManager::loop() { _mqttCore.loop(); }

void MqttManager::setOnCmdCB(OnCmdCB_t cb) { onCmd_cb = cb; }

void MqttManager::setOnStateCB(OnStateCB_t cb) { onState_cb = cb; }

void MqttManager::setOnSwitchCB(OnSwitchCB_t cb) { onSwitch_cb = cb; }

void MqttManager::sendDiscoveries()
{
    char buf[50];

    // 设置settings的Discovery

    _mqttCore.sendDiscoverySwitch("启动", MQTT_PUMP_ON, "mdi:power-cycle", true);
    _mqttCore.sendDiscoveryNumber("水控最短时间", MQTT_WATER_MIN, "mdi:clock-start", "秒", 1, 9, 1, false);
    _mqttCore.sendDiscoveryNumber("水控最长时间", MQTT_WATER_MAX, "mdi:clock-end", "秒", 1, 9, 1, false);
    _mqttCore.sendDiscoveryNumber("单次开泵时长", MQTT_DURATION, "mdi:timer-play", "分", 1, 9, 1, false);
    _mqttCore.sendDiscoveryNumber(
        "保温温度", MQTT_DEMAND_TEMP, "mdi:thermometer", "°C", TEMP_SETTING_LOWEST, TEMP_SETTING_HIGHEST, 1, false);

    char entity_id[20];
    char name[30];
    char val_tpl[50];
    char cmd_tpl[50];
    for (int i = 0; i < 3; i++)
    {
        snprintf(entity_id, sizeof(entity_id), "%s%d", MQTT_END_TIME, i + 1);
        snprintf(name, sizeof(name), "保温%d结束时间", i + 1);
        snprintf(val_tpl, sizeof(val_tpl), "{{ value_json.%s }}:00", entity_id);
        snprintf(cmd_tpl, sizeof(cmd_tpl), "{\"%s\": \"{{ value }}\"}", entity_id);
        _mqttCore.sendDiscoveryTime(name, entity_id, "mdi:calendar-clock", false, true, val_tpl, cmd_tpl, false);

        snprintf(entity_id, sizeof(entity_id), "%s%d", MQTT_START_TIME, i + 1);
        snprintf(name, sizeof(name), "保温%d起始时间", i + 1);
        snprintf(val_tpl, sizeof(val_tpl), "{{ value_json.%s }}:00", entity_id);
        snprintf(cmd_tpl, sizeof(cmd_tpl), "{\"%s\": \"{{ value }}\"}", entity_id);
        _mqttCore.sendDiscoveryTime(name, entity_id, "mdi:calendar-clock", false, true, val_tpl, cmd_tpl, false);
    }

    // 传感器state的Discovery

    _mqttCore.sendDiscoverySensor(
        "当前泵体水温", MQTT_STATE_WATER, "mdi:water-thermometer", "measurement", "°C", false);
    _mqttCore.sendDiscoverySensor("当前流量", MQTT_STATE_FLOW, "mdi:waves-arrow-right", "measurement", "L/min", false);
}

void MqttManager::sendSettings(Settings_t &pumpSettings)
{
    JsonDocument settings;
    char         id[20];
    char         buf[6];

    for (int i = 0; i < 3; i++)
    {
        snprintf(buf, sizeof(buf), "%02d:%02d", pumpSettings.startHour[i], pumpSettings.startMinute[i]);
        snprintf(id, sizeof(id), "%s%d", MQTT_START_TIME, i + 1);
        settings[id] = buf;

        snprintf(buf, sizeof(buf), "%02d:%02d", pumpSettings.endHour[i], pumpSettings.endMinute[i]);
        snprintf(id, sizeof(id), "%s%d", MQTT_END_TIME, i + 1);
        settings[id] = buf;
    }
    settings[MQTT_WATER_MIN]   = pumpSettings.waterMinSec;
    settings[MQTT_WATER_MAX]   = pumpSettings.waterMaxSec;
    settings[MQTT_DURATION]    = pumpSettings.pumpOnDuration;
    settings[MQTT_DEMAND_TEMP] = pumpSettings.demandTemp;

    _mqttCore.sendState(settings);
}

void MqttManager::sendState(State_t &pumpState)
{
    JsonDocument states;

    states[MQTT_STATE_WATER] = pumpState.tempC;
    states[MQTT_STATE_FLOW]  = pumpState.flow;
    states[MQTT_PUMP_ON]     = pumpState.pumpOn ? "ON" : "OFF";

    _mqttCore.sendState(states);
}

void MqttManager::onReceive(const char *topic, const uint8_t *payload, unsigned int len)
{
    if (managerInstance) managerInstance->AnalyzeMsg(topic, payload, len);
}

void MqttManager::AnalyzeMsg(const char *topic, const uint8_t *payload, unsigned int len)
{
    JsonDocument         doc;
    DeserializationError jsonError;
    char                 stateJson[50];
    char                 cmdJson[50];

    // XLOG("MQTT.received.topic", topic);
    // XLOG("MQTT.received.payload", (char *)payload);

    jsonError = deserializeJson(doc, payload);
    if (jsonError) return;

    JsonObject jsonObj = doc.as<JsonObject>();
    snprintf(stateJson, sizeof(stateJson), "hass/%s/state", MQTT_DEV_ABBR);
    snprintf(cmdJson, sizeof(cmdJson), "hass/%s/cmd", MQTT_DEV_ABBR);

    if (strcmp(topic, cmdJson) == 0) handleCmd(jsonObj);
    if (strcmp(topic, stateJson) == 0) handleState(jsonObj);
}

void MqttManager::handleCmd(const JsonObject &obj)
{
    Settings_t set;
    char       buf[256];

    serializeJson(obj, buf, sizeof(buf));

    // 如果收到开关泵指令
    if (obj[MQTT_PUMP_ON].is<String>())
    {
        bool setOn = obj[MQTT_PUMP_ON] == "ON" ? true : false;
        if (onSwitch_cb) onSwitch_cb(setOn);
        return;
    }

    // 如果收到设置参数，更新参数并回调输出（每次只处理一个参数）
    char startTimeKey[30];
    char endTimeKey[30];
    int  h, m, s;
    for (int i = 0; i < 3; i++)
    {
        snprintf(startTimeKey, sizeof(startTimeKey), "%s%d", MQTT_START_TIME, i + 1);
        snprintf(endTimeKey, sizeof(endTimeKey), "%s%d", MQTT_END_TIME, i + 1);

        if (obj[startTimeKey].is<String>())
        {
            const char *startTime = obj[startTimeKey];
            if (startTime && sscanf(startTime, "%d:%d:%d", &h, &m, &s) == 3)
            {
                set.startHour[i]   = h;
                set.startMinute[i] = m;
                if (onCmd_cb) onCmd_cb(set);
            }
            return;
        }

        if (obj[endTimeKey].is<String>())
        {
            const char *endTime = obj[endTimeKey];

            if (endTime && sscanf(endTime, "%d:%d:%d", &h, &m, &s) == 3)
            {
                set.endHour[i]   = h;
                set.endMinute[i] = m;
                if (onCmd_cb) onCmd_cb(set);
            }
            return;
        }
    }

    if (obj[MQTT_WATER_MIN].is<int>())
    {
        set.waterMinSec = obj[MQTT_WATER_MIN].as<int>();
        if (onCmd_cb) onCmd_cb(set);
        return;
    }
    if (obj[MQTT_WATER_MAX].is<int>())
    {
        set.waterMaxSec = obj[MQTT_WATER_MAX].as<int>();
        if (onCmd_cb) onCmd_cb(set);
        return;
    }
    if (obj[MQTT_DURATION].is<int>())
    {
        set.pumpOnDuration = obj[MQTT_DURATION].as<int>();
        if (onCmd_cb) onCmd_cb(set);
        return;
    }
    if (obj[MQTT_DEMAND_TEMP].is<int>())
    {
        set.demandTemp = obj[MQTT_DEMAND_TEMP].as<int>();
        if (onCmd_cb) onCmd_cb(set);
        return;
    }
}

void MqttManager::handleState(const JsonObject &obj)
{
    State_t state;

    if (obj[MQTT_STATE_EXT_WATER].is<float>())
    {
        state.tempC2 = obj[MQTT_STATE_EXT_WATER].as<float>();
        if (onState_cb) onState_cb(state);
    }
}