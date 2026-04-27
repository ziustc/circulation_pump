#pragma once
#include <U8g2lib.h>

// U8G2 字体常量
#define FONT_CHN_16       u8g2_font_wqy16_t_gb2312b   // (16=18x18) 16号中文字体 (12=12x13, 13=12x14, 14=13x16)
#define FONT_CHN_14       u8g2_font_wqy14_t_gb2312b   // (14=13x16) 16号中文字体 (12=12x13, 13=12x14, 14=13x16)
#define FONT_INPUT_DIGIT  u8g2_font_logisoso16_tn     // (9x19) 输入数值字体
#define FONT_DISP_SMALL   u8g2_font_fub14_tf          // (21x26) 显示数值字体 小号
#define FONT_DISP_MID     u8g2_font_fub20_tf          // (28x36) 显示数值字体 中号
#define FONT_DISP_LARGE   u8g2_font_fub30_tf          // (41x54) 显示数值字体 大号
#define FONT_DISP_SMALL2  u8g2_font_VCR_OSD_tn        // (11x16) 显示数值字体2 小号
#define FONT_NARROW_SMALL u8g2_font_busdisplay11x5_tr // (5x13) 窄字体 小号
#define FONT_SMALL_ENG    u8g2_font_profont11_tr      // (6x11) 小英文数字
#define FONT_MID_ENG      u8g2_font_profont15_tr      // (7x13) 中英文数字
#define FONT_SMALL_ENG2   u8g2_font_bitcasual_tr      // (11x11) 小英文数字

// 控制器常量
#define TEMP_SETTING_LOWEST    20  // 设定温度最低值
#define TEMP_SETTING_HIGHEST   45  // 设定温度最高值
#define TEMP_LOWER_MARGIN      0   // 单位：°C，温控低于设定值多少即开泵
#define TEMP_UPPER_MARGIN      2   // 单位：°C，温控超过设定值多少即关泵
#define TEMP_OVERTIME_LIMIT    10  // 单位：分钟，温控开泵超过多少分钟强制关泵
#define TEMP_OVERTIME_RECOVERY 10  // 单位：分钟，温控强制关泵后多少分钟允许再次开泵，单位分钟
#define WATER_MARGIN           3   // 单位：秒，避免上次关泵后水流未停止，水控需在上次关泵后一段时间才能触发
#define FLOW_THRESHOLD         3.0 // 单位：L/min，判定水管打开的流量阈值
#define SENSOR_FREQ            500 // 500ms读取一次传感器

// MQTT常量
#define MQTT_STATE_FREQ   10 * 1000      // 10s发送一次settings和state到HASS
#define MQTT_SETTING_FREQ 10 * 60 * 1000 // 10分钟发送一次Discovery到HASS

struct Settings_t
{
    int startHour[3]   = {-1, -1, -1};
    int startMinute[3] = {-1, -1, -1};
    int endHour[3]     = {-1, -1, -1};
    int endMinute[3]   = {-1, -1, -1};
    int waterMinSec    = -1;
    int waterMaxSec    = -1;
    int pumpOnDuration = -1;
    int demandTemp     = -1;
};

struct State_t
{
    int   tempC  = 0; // 泵体水温
    int   tempC2 = 0; // 用水端水温
    float flow   = 0;
    bool  pumpOn = 0;
};
