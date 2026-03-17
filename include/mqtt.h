#pragma once

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "screen.h" // 包含 Settings 结构体定义

// 回调函数类型
typedef void (*OnMsgCallback_t)(Settings set);
typedef void (*OnPumpOnCallback_t)();

class PumpMqttManager
{
public:
    PumpMqttManager();

    void begin();
    void loop();
    void reconnect();
    void setOnMsgCallback(OnMsgCallback_t cb);
    void setPumpOnCallback(OnPumpOnCallback_t cb);
    void sendMsg(const char *topic, const char *payload);

    void sendDiscoveries();
    void sendState(Settings &pumpSettings);

private:
    static PumpMqttManager *instance; // 保存对象指针

    WiFiClient         _wifiClient;
    PubSubClient       _client;
    OnMsgCallback_t    _onMqttMsg_cb;
    OnPumpOnCallback_t _onMqttPumpOn_cb;

    static void mqtt_received(const char *topic, const uint8_t *payload, unsigned int len);
    void        handleCommand(const char *topic, const uint8_t *payload, unsigned int len);

    static String timeDiscoveryPayload(String entityName, String entityId);
    static String
    numberDiscoveryPayload(String entityName, String entityId, const char *unit, int min, int max, int step = 1);
    static String       buttonDiscoveryPayload(String entityName, String entityId);
    static JsonDocument DiscoveryPayloadTemplate(String entityName, String entityId);
    static String       statePayload(Settings &pumpSettings);
};