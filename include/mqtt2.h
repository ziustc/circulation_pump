#pragma once

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <vector>
#include "common.h"
#include "extlogger.h"
#include "ssid.h"

using namespace std;

struct MqttInitParams_t
{
    const char *clientId;
    const char *server  = "192.168.0.20";
    uint16_t    port    = 1883;
    int         bufSize = 1024;
};

struct DeviceInfo_t
{
    const char *identifier;
    const char *name;    // 设备中文名
    const char *id_abbr; // 设备名缩写（英文），用于生成设备统一的cmd/state topic
    const char *manufacturer;
    const char *model;
    const char *viaDevice;
    char        swVersion[10];
    char        hwVersion[10];
};

// MqttCore回调MqttManager的回调函数类型
typedef void (*OnMqttReceive_t)(const char *topic, JsonObject &obj);

class MqttCore
{
public:
    MqttCore();
    void init(MqttInitParams_t mqttInfo, DeviceInfo_t devInfo, OnMqttReceive_t cb);
    void loop();
    bool sendDiscoveryBase(const char         *type,
                           const char         *entityName,
                           const char         *entityId,
                           const char         *icon,
                           const JsonDocument &attr,
                           bool                fullDevInfo);

    bool sendDiscoveryNumber(const char *entityName,
                             const char *entityId,
                             const char *icon,
                             const char *unit,
                             int         min,
                             int         max,
                             int         step,
                             bool        fullDevInfo);

    bool sendDiscoverySwitch(const char *entityName, const char *entityId, const char *icon, bool fullDevInfo);

    bool sendDiscoveryTime(const char *entityName,
                           const char *entityId,
                           const char *icon,
                           bool        has_date,
                           bool        has_time,
                           const char *val_tpl,
                           const char *cmd_tpl,
                           bool        fullDevInfo);

    bool sendDiscoverySensor(const char *entityName,
                             const char *entityId,
                             const char *icon,
                             const char *stat_cla,
                             const char *unit_of_meas,
                             bool        fullDevInfo);

    bool sendState(const JsonDocument &states);

private:
    static MqttCore *coreInstance; // 本对象指针
    WiFiClient       _wifiClient;
    PubSubClient     _client;
    MqttInitParams_t _mqttInfo;
    DeviceInfo_t     _devInfo;
    char             _stateJson[50];
    char             _cmdJson[50];
    OnMqttReceive_t  onReceive_cb;

    static void onReceive(const char *topic, const uint8_t *payload, unsigned int len);
    void        reconnect();
};

// MqttManager回调PCU的函数类型
typedef void (*OnCmdCB_t)(Settings_t set);
typedef void (*OnStateCB_t)(State_t state);
typedef void (*OnSwitchCB_t)(bool setOn);

class MqttManager
{
public:
    MqttManager();
    void init();
    void loop();

    void setOnCmdCB(OnCmdCB_t cb);
    void setOnStateCB(OnStateCB_t cb);
    void setOnSwitchCB(OnSwitchCB_t cb);

    void sendDiscoveries();

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
    static MqttManager *managerInstance; // 本对象指针
    MqttCore            _mqttCore;

    OnCmdCB_t    onCmd_cb;
    OnStateCB_t  onState_cb;
    OnSwitchCB_t onSwitch_cb;

    static void onReceive(const char *topic, JsonObject &obj);
    void        AnalyzeMsg(const char *topic, JsonObject &obj);
    void        handleCmd(const JsonObject &obj);
    void        handleState(const JsonObject &obj);
};
