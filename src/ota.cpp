#include <Arduino.h>
#include <Preferences.h>
#include "ota.h"

#define PREF_OTA_NEW     1
#define PREF_OTA_PENDING 2
#define PREF_OTA_STABLE  3

Preferences prefs;

int OtaAssist::_status = PREF_OTA_NEW;

void OtaAssist::init()
{
    // ---- ArduinoOTA 初始化 ----
    ArduinoOTA.begin();

    // ---- 判断 OTA 状态 ----
    esp_partition_t     *running, *alter;
    esp_ota_img_states_t state;

    running = (esp_partition_t *)esp_ota_get_running_partition();
    alter   = (esp_partition_t *)esp_ota_get_next_update_partition(running);
    esp_ota_get_state_partition(running, &state);

    Serial.printf("[OTA] running in %s app...", strSubtype(running->subtype));
    Serial.println(strImgState(state));

    // 记录当前OTA数据到NVS，并确认是否需要回滚
    if (nvsCheckRollback(running->subtype))
    {
        Serial.println("[OTA] app unstable, preparing rollback");
        for (int i = 5; i > 0; i--)
        {
            Serial.printf("[OTA] OTA rollback in %d s", i);
            delay(1000);
        }
        esp_restart();
    }
}

void OtaAssist::clearOtaData()
{
    prefs.begin("ota", false);
    prefs.clear();
    prefs.end();
}

void OtaAssist::handle() { ArduinoOTA.handle(); }

void OtaAssist::confirm()
{
    if (_status == PREF_OTA_PENDING)
    {
        prefs.begin("ota", false);
        prefs.putInt("OTA_STATUS", PREF_OTA_STABLE);
        _status = PREF_OTA_STABLE;
        prefs.end();
        Serial.println("[OTA] APP confirmed stable");
    }
}

bool OtaAssist::isStable() { return (_status == PREF_OTA_STABLE); }

String OtaAssist::strImgState(esp_ota_img_states_t state)
{
    switch (state)
    {
    case ESP_OTA_IMG_NEW:
        return "ESP_OTA_IMG_NEW";
    case ESP_OTA_IMG_PENDING_VERIFY:
        return "ESP_OTA_IMG_PENDING_VERIFY";
    case ESP_OTA_IMG_VALID:
        return "ESP_OTA_IMG_VALID";
    case ESP_OTA_IMG_INVALID:
        return "ESP_OTA_IMG_INVALID";
    case ESP_OTA_IMG_ABORTED:
        return "ESP_OTA_IMG_ABORTED";
    case ESP_OTA_IMG_UNDEFINED:
        return "ESP_OTA_IMG_UNDEFINED";
    default:
        return "ESP_OTA_UNKNOWN";
    }
}

String OtaAssist::strSubtype(esp_partition_subtype_t subtype)
{
    switch (subtype)
    {
    case ESP_PARTITION_SUBTYPE_APP_FACTORY:
        return "FACTORY";
    case ESP_PARTITION_SUBTYPE_APP_OTA_0:
        return "OTA_0";
    case ESP_PARTITION_SUBTYPE_APP_OTA_1:
        return "OTA_1";
    default:
        return "UNKNOWN";
    }
}

bool OtaAssist::nvsCheckRollback(esp_partition_subtype_t subtype)
{
    prefs.begin("ota", false);

    // 确认当前运行的分区与上次记录的是否一致，若不一致，则定义为首次运行，且记录本次分区
    if (subtype != prefs.getInt("OTA_SUBTYPE", ESP_PARTITION_SUBTYPE_APP_FACTORY))
    {
        Serial.println("[OTA] new app");
        _status = PREF_OTA_NEW;
        prefs.putInt("OTA_SUBTYPE", subtype);
    }
    // 若一致，则读取当前OTA状态（首次运行/待确认/稳定），用_status变量记录
    else
    {
        _status = prefs.getInt("OTA_STATUS", PREF_OTA_NEW);
    }

    // 根据当前OTA状态，决定如何处理本次运行
    switch (_status)
    {
    // 如果是第一次运行，则本次定义为Pending
    case PREF_OTA_NEW:
        prefs.putInt("OTA_STATUS", PREF_OTA_PENDING);
        _status = PREF_OTA_PENDING;
        Serial.println("[OTA] app pending for verification");
        break;

    // 如果本来已经是Pending状态，由重启运行到这里，说明不稳定，需要回滚
    case PREF_OTA_PENDING:
        prefs.clear();
        prefs.end();
        return true;

    // 如果已经确认为Stable，则继续运行
    default: // PREF_OTA_STABLE
        Serial.println("[OTA] app stable");
    }

    prefs.end();
    return false;
}
