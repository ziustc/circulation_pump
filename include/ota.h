/**
 * 
 * 根目录下新建partitions.csv文件，内容如下
 * -----------------------------------------------
 # Name,   Type, SubType, Offset, Size, Flags
 nvs,      data, nvs,     ,       0x5000,
 otadata,  data, ota,     ,       0x2000,
 app0,     app,  ota_0,   ,       0x300000,
 app1,     app,  ota_1,   ,       0x300000,
 spiffs,   data, spiffs,  ,       0x100000,
 * -----------------------------------------------
 * 
 * platformio.ini 增加内容如下
 * -----------------------------------------------
 ; 指定分区表
board_build.partitions = partitions.csv

; 使用platformio自带的烧录按键（"->"键）进行OTA下载
; 首次用USB下载时，以下两条应打上注释
; 以后使用OTA下载，取消注释并改成实际IP
upload_protocol= espota
upload_port = 192.168.0.197
 * -----------------------------------------------
 * 
 * main.cpp中调用如下
 * -----------------------------------------------
 OtaAssist ota;
 setup()
 {
    // 先连接wifi
    ota.init();
    // 其他
 }
 
 loop()
 {
    // 随时响应OTA请求
    ota.handle();
    
    // 在确认固件稳定时调用confirm，在confirm之前若重启，则回滚到上一版本
    if (!ota.isStable() && 确认固件稳定)
    {
      ota.confirm();
    }
 }
 * ------------------------------------------------
 */

#pragma once

#include <Arduino.h>
#include <ArduinoOTA.h>
#include "esp_ota_ops.h"
#include "esp_system.h"

class OtaAssist
{
public:
    /**
     * 读取当前OTA状态，并为下次OTA做准备，在setup()中尽早调用，但需要WIFI连接成功后才能调用
     */
    static void init();

    /**
     * 若本次为非OTA升级，则在init()基础上，应增加调用本函数；否则务必注释掉本函数
     */
    static void clearOtaData();

    /**
     * 随时响应OTA需求，在loop()循环中调用
     */
    static void handle();

    /**
     * 每次OTA后，确认本固件版本稳定时调用
     */
    static void confirm();

    /**
     * 确认本固件版本是否已标记为稳定
     */
    static bool isStable();

private:
    static int    _status;
    static String strSubtype(esp_partition_subtype_t subtype);
    static String strImgState(esp_ota_img_states_t state);
    static bool   nvsCheckRollback(esp_partition_subtype_t subtype);
};
