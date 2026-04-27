#pragma once

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "common.h"

#define CLIENT_ID "CirculationPump"

// 回调函数类型
typedef void (*OnCmdCallback_t)(Settings_t set);
typedef void (*OnStateCallback_t)(State_t state);
typedef void (*OnPumpOnCallback_t)();

class PumpMqttManager
{
public:
    PumpMqttManager();

    void init();
    void loop();
    void reconnect();
    void setOnCmdCallback(OnCmdCallback_t cb);
    void setOnStateCallback(OnStateCallback_t cb);
    void setPumpOnCallback(OnPumpOnCallback_t cb);
    void sendMsg(const char *topic, const char *payload);

    /**
     * 发送HASS自发现消息
     * @return 成功发送的entity数量
     */
    int sendDiscoveries();

    /**
     * 发送泵设置消息
     * @param pumpSettings Settings_t结构体，包含泵的设置参数
     */
    void sendSettings(Settings_t &pumpSettings);

    /**
     * 发送泵状态消息
     * @param pumpState State_t结构体，包含泵的状态参数
     */
    void sendState(State_t &pumpState);

private:
    static PumpMqttManager *instance; // 保存对象指针

    WiFiClient         _wifiClient;
    PubSubClient       _client;
    OnCmdCallback_t    _onMqttCmd_cb;
    OnStateCallback_t  _onMqttState_cb;
    OnPumpOnCallback_t _onMqttPumpOn_cb;

    static void onReceive(const char *topic, const uint8_t *payload, unsigned int len);
    void        receiveMsg(const char *topic, const uint8_t *payload, unsigned int len);
    void        handleCmd(const JsonObject &obj);
    void        handleState(const JsonObject &obj);

    static String timeDiscoveryPayload(String entityName, String entityId);
    static String
    numberDiscoveryPayload(String entityName, String entityId, const char *unit, int min, int max, int step = 1);
    static String       waterDiscoveryPayload(String entityName, String entityId);
    static String       flowDiscoveryPayload(String entityName, String entityId);
    static String       StateOnDiscoveryPayload(String entityName, String entityId);
    static String       buttonDiscoveryPayload(String entityName, String entityId);
    static JsonDocument DiscoveryPayloadTemplate(String entityName, String entityId);
    static String       settingsPayload(Settings_t &pumpSettings);
    static String       statePayload(State_t &pumpState);
};