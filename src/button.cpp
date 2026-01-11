#include <functional>
#include <Arduino.h>
#include "button.h"

using namespace std;

#define DEBOUNCE_MS   50   // 防抖时间
#define LONG_PRESS_MS 1000 // 长按启动时间
#define REPEAT_MS     300  // 长按连续触发间隔

Button::Button()
{
    lastStableState  = false;
    lastReading      = false;
    lastChangeMillis = 0;
    firstClickTime   = 0;
    lastClickTime    = 0;
    callbackClick    = nullptr;
}

void Button::setCallbackClick(function<void()> cbClick) { callbackClick = cbClick; }

void Button::setTouchPin(int pinNum) { buttonPinNum = pinNum; }
int  Button::getTouchPin() { return buttonPinNum; }

/**
 * @brief 带防抖功能的按键检测函数，主循环中调用，识别单击和长按
 */
void Button::stateTick(int pinValue)
{
    unsigned long now      = millis();
    bool          isPinSet = readTP(pinValue);

    // 检测电平状态但不立即相应
    if (isPinSet != lastReading)
    {
        lastReading      = isPinSet;
        lastChangeMillis = now;
    }

    // 消除抖动后仍然检测到电平变化
    if ((now - lastChangeMillis) > DEBOUNCE_MS && lastReading != lastStableState)
    {
        lastStableState = lastReading;

        // 检测到按下动作，触发单击，且准备长按
        if (lastStableState)
        {
            onSingleClick();
            firstClickTime = now;
        }
        // 检测到松开动作，重置长按状态
        else
        {
            firstClickTime = 0;
            lastClickTime  = 0;
        }
    }

    // 若持续按下状态超过阈值，则触发长按事件
    if (lastStableState && (now - firstClickTime) > LONG_PRESS_MS)
    {
        onLongPress(); // 每次调用都触发一次长按事件
    }
}

void Button::onSingleClick()
{
    if (callbackClick != nullptr) callbackClick();
}

void Button::onLongPress()
{
    unsigned long now = millis();

    if ((now - lastClickTime) < REPEAT_MS) return;

    if (callbackClick != nullptr) callbackClick();

    lastClickTime = now;
}

bool Button::readTP(int pinValue)
{
    bool ret;

    // 校准阶段，收集初始数据
    if (!tpCalibrated)
    {
        pinValues[tpIndex] = pinValue;
        pinSum += pinValue;

        tpIndex++;
        if (tpIndex >= TP_MAX_ARR)
        {
            tpCalibrated = true;
            tpIndex      = 0;
        }
        return false;
    }

    // 触摸状态
    if (pinValue * TP_MAX_ARR > pinSum * TP_THRESHOLD)
    {
        ret = true;
        // 不更新平均值，保持稳定
    }
    // 非触摸状态，更新平均值
    else
    {
        pinSum             = pinSum - pinValues[tpIndex] + pinValue;
        pinValues[tpIndex] = pinValue;
        ret                = false;
        tpIndex            = (tpIndex + 1) % TP_MAX_ARR;
    }
    return ret;
}