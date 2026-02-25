#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdate.h>
#include <esp_ota_ops.h>

class OtaAssist
{
public:
    OtaAssist(uint16_t port = 80);

    /**
     * 读取当前OTA状态，并为下次OTA做准备，在setup()中尽早调用，但需要WIFI连接成功后才能调用
     */
    void init();

    /**
     * 随时响应OTA需求，在loop()循环中调用
     */
    void loop();

    /**
     * 设置当前固件版本号，便于OTA后在日志中确认版本信息
     */
    void setVersion(const String &ver);

    /**
     * 获取当前固件版本号
     */
    String getVersion() const;

    /**
     * 若本次为非OTA升级，则在init()基础上，应增加调用本函数；否则务必注释掉本函数
     */
    static void clearOtaData();

    /**
     * 每次OTA后，确认本固件版本稳定时调用
     */
    static void confirm();

    /**
     * 确认本固件版本是否已标记为稳定
     */
    static bool isStable();

private:
    WebServer  _server;
    String     _version;
    static int _status;

    // 以下为OTA过程处理函数
    void          handleRoot();
    void          handleUpdate();
    static void   printProgressBar(size_t current, size_t total);
    static String formatSize(size_t bytes);

    // 以下为检查OTA版本状态和回滚的函数
    static String strSubtype(esp_partition_subtype_t subtype);
    static String strImgState(esp_ota_img_states_t state);
    static bool   checkRollback(esp_partition_subtype_t subtype);
};
