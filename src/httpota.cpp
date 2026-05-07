#include "httpota.h"
#include <esp_ota_ops.h>
#include <math.h>
#include "extlogger.h"

/** _prefs("ota")中有如下NVS信息：
 * "VERSION": String，当前固件版本号字符串
 * "APP_STATUS": Int，当前OTA状态
 * "VERIFY_TIMES": Int，当前OTA状态为PENDING时的重启验证次数，超过3次则认为不稳定，回滚到上一个版本
 * "STABLE_SUBTYPE": Int，上一个稳定版本的分区subtype，PREF_OTA_INVALID时回滚到该分区
 * "STABLE_VERSION": String，上一个稳定版本的版本号字符串，回滚后复制
 */

OtaAssist::OtaAssist(uint16_t port)
: _server(port)
, _version("unknown")
, _prefs()
{
}

void OtaAssist::stableCheck()
{
    // 本函数在启动最前面，还没有logger，只能用串口输出日志

    esp_ota_img_states_t state;

    _running = (esp_partition_t *)esp_ota_get_running_partition();
    _prefs.begin("ota", false);
    _version = _prefs.getString("VERSION", "unknown");
    _status  = static_cast<OtaStatus_t>(_prefs.getInt(
        "APP_STATUS", static_cast<int>(OtaStatus_t::UART_STABLE))); // 如果没有记录，则一定是串口烧录，按稳定处理
    XLOG("OTA", "current partition: %s, version: %s", strSubtype(_running->subtype).c_str(), _version.c_str());

    // 如果当前分区状态为UART烧录，则默认稳定，无需处理
    if (_status == OtaStatus_t::UART_STABLE)
    {
        _status = OtaStatus_t::UART_STABLE;
        _prefs.putInt("APP_STATUS", static_cast<int>(OtaStatus_t::UART_STABLE));
        _prefs.putInt("STABLE_SUBTYPE", _running->subtype);
        _prefs.putString("STABLE_VERSION", _version);
        _prefs.end();
        XLOG("OTA", "firmware stable (UART)");
        return;
    }

    // 如果当前分区状态为稳定，则无需处理
    if (_status == OtaStatus_t::OTA_STABLE)
    {
        _prefs.end();
        XLOG("OTA", "firmware stable");
        return;
    }

    // 否则说明本次是OTA升级待验证，状态为PENDING
    if (_status == OtaStatus_t::OTA_NEW)
    {
        _status = OtaStatus_t::OTA_PENDING;
        _prefs.putInt("APP_STATUS", static_cast<int>(OtaStatus_t::OTA_PENDING));
    }

    // 如果PENDING状态不超过3次重启，则仍然标记为PENDING，记录重启次数+1
    int verifyTimes = _prefs.getInt("VERIFY_TIMES", 0);
    if (verifyTimes < 3)
    {
        _prefs.putInt("VERIFY_TIMES", verifyTimes + 1);
        _prefs.end();
        XLOG("OTA", "firmware pending verification. Attempt %d times.", verifyTimes + 1);
        return;
    }

    // PENDING超过3次重启，说明本次固件版本不稳定，状态为INVALID，回滚到上一个版本
    _status = OtaStatus_t::OTA_INVALID;

    // 回滚重启
    // ----如果记录有上次稳定版本，则回滚到上次稳定版本，否则找到下一个可用分区回滚
    esp_partition_subtype_t lastStableSubtype =
        (esp_partition_subtype_t)_prefs.getInt("STABLE_SUBTYPE", ESP_PARTITION_SUBTYPE_APP_FACTORY);
    if (lastStableSubtype != ESP_PARTITION_SUBTYPE_APP_FACTORY)
        _last = (esp_partition_t *)esp_partition_find_first(ESP_PARTITION_TYPE_APP, lastStableSubtype, NULL);
    else
        _last = (esp_partition_t *)esp_ota_get_next_update_partition(_running);
    esp_ota_set_boot_partition(_last);

    // ----NVS回到上次稳定版本的信息
    _prefs.putString("VERSION", _prefs.getString("STABLE_VERSION", "unknown"));
    _prefs.putInt("APP_STATUS", static_cast<int>(OtaStatus_t::OTA_NEW));
    _prefs.putInt("VERIFY_TIMES", 0);

    // ----重启
    for (int i = 5; i > 0; i--)
    {
        XLOG("OTA", "firmware unstable, rollback in %d s", i);
        delay(1000);
    }
    esp_restart();
}

void OtaAssist::initService()
{
    // 启动http server，等待PC端给出OTA指令
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/update", HTTP_GET, [this]() { handleUpdate(); });
    _server.begin();

    // 准备就绪

    XLOG("OTA",
         "current partition = %s, version = %s, status = %s",
         strSubtype(_running->subtype).c_str(),
         _version.c_str(),
         strStatus(_status).c_str());
}

void OtaAssist::loop() { _server.handleClient(); }

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

    XLOG("OTA", " downloading from: %s", url.c_str());

    WiFiClient client;

    httpUpdate.onProgress([&](int current, int total) { updateProgBar(current, total); });
    // httpUpdate.onEnd([&]() { otaOnEnd(); });
    httpUpdate.update(client, url);

    return;
}

void OtaAssist::updateProgBar(size_t current, size_t total)
{
    const int barWidth = 40;

    float progress = (float)current / total;
    int   pos      = barWidth * progress;

    // 高频率更新进度条，避免过多日志输出，仅用串口
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

    // 由于结束时还没来得及完成写入状态等工作就被重启了，所以在这里直接调用写入状态函数，确保NVS信息正确更新
    if (current == total) otaOnEnd();
}

void OtaAssist::otaOnEnd()
{
    _prefs.begin("ota", false);
    _prefs.putString("VERSION", _version);
    _prefs.putInt("APP_STATUS", static_cast<int>(OtaStatus_t::OTA_NEW));
    _prefs.putInt("VERIFY_TIMES", 0);
    _prefs.end();
    _status = OtaStatus_t::OTA_NEW;
    XLOG("OTA", "Update finished, Rebooting...");
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
    _prefs.begin("ota", false);
    _prefs.clear();
    _prefs.end();
}

void OtaAssist::confirm()
{
    if (_status == OtaStatus_t::OTA_PENDING)
    {
        _prefs.begin("ota", false);
        _prefs.putInt("APP_STATUS", static_cast<int>(OtaStatus_t::OTA_STABLE));
        _prefs.putInt("VERIFY_TIMES", 0);
        _prefs.putInt("STABLE_SUBTYPE", _running->subtype);
        _prefs.putString("STABLE_VERSION", _version);
        _prefs.end();
        _status = OtaStatus_t::OTA_STABLE;
        XLOG("OTA", "firmware confirmed stable");
    }
}

bool OtaAssist::isStable() { return (_status == OtaStatus_t::OTA_STABLE && _status == OtaStatus_t::UART_STABLE); }

String OtaAssist::strStatus(OtaStatus_t status)
{
    switch (status)
    {
    case OtaStatus_t::OTA_NEW:
        return "OTA_NEW";
    case OtaStatus_t::OTA_PENDING:
        return "OTA_PENDING";
    case OtaStatus_t::OTA_STABLE:
        return "OTA_STABLE";
    case OtaStatus_t::OTA_INVALID:
        return "OTA_INVALID";
    case OtaStatus_t::UART_STABLE:
        return "UART_STABLE";
    default:
        return "PREF_OTA_UNKNOWN";
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
    case ESP_PARTITION_SUBTYPE_APP_OTA_2:
        return "OTA_2";
    default:
        return "UNKNOWN";
    }
}
