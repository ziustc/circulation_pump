#pragma once

#include <functional>

#define TP_MAX_ARR   32    // TouchPin采样数组长度
#define TP_THRESHOLD 1.05f // TouchPin按下阈值，超过长期平均值x这个系数以上则视为按下

#define DEBOUNCE_MS        50   // 防抖时间
#define LONG_PRESS_MS      1000 // 长按启动时间（用于长按连续触发）
#define REPEAT_MS          200  // 长按连续触发间隔
#define VERY_LONG_PRESS_MS 3000 // 超长按启动时间（用于切换功能，如童锁，关机等）
#define BEEP_DURATION_MS   50   // 蜂鸣器响铃持续时间

using namespace std;

class Button
{
public:
    Button();
    /**
     * 长按和超长按，只能选其一
     */
    void setLongPressEnable(bool enable);
    bool getLongPressEnable();
    /**
     * 长按和超长按，只能选其一
     */
    void setVeryLongPressEnable(bool enable);
    bool getVeryLongPressEnable();
    void setClickCallback(function<void()> cbClick);
    void setVeryLongPressCallback(function<void()> cbAlterPress) { alterPress_cb = cbAlterPress; }
    void setTouchPin(int pinNum);
    int  getTouchPin();
    void setBeepPin(int pinNum) { beepPinNum = pinNum; }
    void stateTick(int pinValue);

private:
    int              buttonPinNum;
    int              beepPinNum;
    int              pinValues[32] = {0};
    int              pinSum        = 0;
    int              tpIndex       = 0;
    bool             lpEnable      = false;
    bool             vlpEnable     = false;
    bool             tpCalibrated  = false;
    bool             lastStableState;
    bool             lastReading;
    unsigned long    lastChangeMillis;
    unsigned long    beepOnMillis;
    unsigned long    firstClickTime; // 首次按下的时间，用于判断长按，与第一次onSingleClick时间相同
    unsigned long    lastClickTime;  // 持续长按时，上次触发click的时间
    function<void()> click_cb;
    function<void()> alterPress_cb; // 用于切换点击和超长按功能的回调
    void             onSingleClick();
    void             onLongPress();
    void             onVeryLongPress();
    bool             readTP(int pinValue); // 根据TouchPin的数值，识别按键按下状态
};
