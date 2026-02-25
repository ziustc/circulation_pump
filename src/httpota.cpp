#include "httpota.h"
#include <esp_ota_ops.h>
#include <Preferences.h>
#include <math.h>

#define PREF_OTA_NEW     1
#define PREF_OTA_PENDING 2
#define PREF_OTA_STABLE  3

Preferences prefs;

int OtaAssist::_status = PREF_OTA_NEW;

OtaAssist::OtaAssist(uint16_t port)
: _server(port)
, _version("unknown")
{
}

void OtaAssist::init()
{
    // 启动http server，等待PC端给出OTA指令
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/update", HTTP_GET, [this]() { handleUpdate(); });
    _server.begin();

    // 判断当前运行版本状态
    esp_partition_t     *running, *alter;
    esp_ota_img_states_t state;

    running = (esp_partition_t *)esp_ota_get_running_partition();
    alter   = (esp_partition_t *)esp_ota_get_next_update_partition(running);
    esp_ota_get_state_partition(running, &state);

    Serial.printf("[OTA] running in %s, version %s...", strSubtype(running->subtype), _version.c_str());
    Serial.println(strImgState(state));

    // 记录当前OTA数据到NVS，并确认是否需要回滚
    if (checkRollback(running->subtype))
    {
        Serial.println("[OTA] firmware unstable, preparing rollback");
        for (int i = 5; i > 0; i--)
        {
            Serial.printf("[OTA] firmware rollback in %d s", i);
            delay(1000);
        }
        esp_restart();
    }

    // 准备就绪
    Serial.println("[OTA] ready for next OTA");
}

void OtaAssist::loop() { _server.handleClient(); }

void OtaAssist::setVersion(const String &ver) { _version = ver; }

String OtaAssist::getVersion() const { return _version; }

void OtaAssist::handleRoot()
{
    String info = "Device OK\nVersion: " + _version;
    _server.send(200, "text/plain", info);
}

void OtaAssist::handleUpdate()
{
    if (!_server.hasArg("url"))
    {
        _server.send(400, "text/plain", "Missing url parameter");
        return;
    }

    String url = _server.arg("url");

    Serial.print("[OTA] downloading from: ");
    Serial.println(url);

    WiFiClient client;

    httpUpdate.onProgress([&](int current, int total) { printProgressBar(current, total); });

    t_httpUpdate_return ret = httpUpdate.update(client, url);

    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
        Serial.printf("[OTA] Failed: %s\n", httpUpdate.getLastErrorString().c_str());
        _server.send(500, "text/plain", "Update Failed");
        break;

    case HTTP_UPDATE_NO_UPDATES:
        Serial.println("[OTA] No Updates");
        _server.send(200, "text/plain", "No Update");
        break;

    case HTTP_UPDATE_OK:
        Serial.println("[OTA] Update OK, Rebooting...");
        // 不需要 send，update 成功后会自动重启
        break;
    }
}

void OtaAssist::printProgressBar(size_t current, size_t total)
{
    const int barWidth = 40;

    float progress = (float)current / total;
    int   pos      = barWidth * progress;

    Serial.print("[");

    for (int i = 0; i < barWidth; i++)
    {
        if (i < pos)
            Serial.print("=");
        else if (i == pos)
            Serial.print(">");
        else
            Serial.print(" ");
    }

    Serial.print("] ");

    // 百分比
    Serial.printf("%3d%%  ", (int)(progress * 100));

    // 字节信息
    Serial.print(formatSize(current));
    Serial.print(" / ");
    Serial.println(formatSize(total));
}

String OtaAssist::formatSize(size_t bytes)
{
    const char *units[]   = {"B", "kB", "MB"};
    float       size      = bytes;
    int         unitIndex = 0;

    while (size >= 1000 && unitIndex < 2)
    {
        size /= 1000;
        unitIndex++;
    }

    char buffer[10];
    // 3 位有效数字
    snprintf(buffer, sizeof(buffer), "%.3g %s", size, units[unitIndex]);
    return String(buffer);
}

void OtaAssist::clearOtaData()
{
    prefs.begin("ota", false);
    prefs.clear();
    prefs.end();
}

void OtaAssist::confirm()
{
    if (_status == PREF_OTA_PENDING)
    {
        prefs.begin("ota", false);
        prefs.putInt("OTA_STATUS", PREF_OTA_STABLE);
        _status = PREF_OTA_STABLE;
        prefs.end();
        Serial.println("[OTA] firmware confirmed stable");
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

bool OtaAssist::checkRollback(esp_partition_subtype_t subtype)
{
    prefs.begin("ota", false);

    // 确认当前运行的分区与上次记录的是否一致，若不一致，则定义为首次运行，且记录本次分区
    if (subtype != prefs.getInt("OTA_SUBTYPE", ESP_PARTITION_SUBTYPE_APP_FACTORY))
    {
        Serial.println("[OTA] new firmware");
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
        Serial.println("[OTA] firmware pending for verification");
        break;

    // 如果本来已经是Pending状态，由重启运行到这里，说明不稳定，需要回滚
    case PREF_OTA_PENDING:
        prefs.clear();
        prefs.end();
        return true;

    // 如果已经确认为Stable，则继续运行
    default: // PREF_OTA_STABLE
        Serial.println("[OTA] firmware stable");
    }

    prefs.end();
    return false;
}
