#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdate.h>
#include <esp_ota_ops.h>

class OtaAssist
{
public:
    OtaAssist(uint16_t port = 80);

    /**
     * setup()最初调用，判断当前固件版本是否稳定，若不稳定则回滚
     * 要求在setup()最前面，初始化Serial后立即调用
     */
    void stableCheck();

    /**
     * 启动HTTP OTA服务器
     */
    void initService();

    /**
     * 随时响应OTA需求，在loop()循环中调用
     */
    void loop();

    /**
     * 获取当前固件版本号
     */
    String getVersion() const;

    /**
     * 若本次为非OTA升级，则在stableCheck()基础上，应增加调用本函数；否则务必注释掉本函数
     */
    void clearOtaData();

    /**
     * 每次OTA后，确认本固件版本稳定时调用
     */
    void confirm();

    /**
     * 确认本固件版本是否已标记为稳定
     */
    bool isStable();

private:
    enum class OtaStatus_t
    {
        OTA_NEW     = 1, // 刚OTA升级完成，待验证
        OTA_PENDING = 2, // 验证过程中，重启过一次以上但未超过3次
        OTA_STABLE  = 3, // 验证通过，稳定版本
        OTA_INVALID = 4, // 验证失败，回滚到上一个稳定版本
        UART_STABLE = 5  // 串口烧录的版本，默认稳定
    };
    Preferences      _prefs;
    WebServer        _server;
    OtaStatus_t      _status;
    esp_partition_t *_running, *_last;

    // 以下为OTA过程处理函数
    void          handleRoot();
    void          handleUpdate();
    void          updateProgBar(size_t current, size_t total);
    void          otaOnEnd();
    static String formatSize(size_t bytes);

    // 以下为检查OTA版本状态和回滚的函数
    static String strSubtype(esp_partition_subtype_t subtype);
    static String strStatus(OtaStatus_t status);
};
