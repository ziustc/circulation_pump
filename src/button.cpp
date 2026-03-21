#include <functional>
#include <Arduino.h>
#include "button.h"
#include "extlogger.h"

using namespace std;

Button::Button()
{
    lastStableState  = false;
    lastReading      = false;
    lastChangeMillis = 0;
    firstClickTime   = 0;
    lastClickTime    = 0;
    click_cb         = nullptr;
}

void Button::setLongPressEnable(bool enable)
{
    if (enable) setVeryLongPressEnable(false);
    lpEnable = enable;
}

bool Button::getLongPressEnable() { return lpEnable; }

void Button::setVeryLongPressEnable(bool enable)
{
    if (enable) setLongPressEnable(false);
    vlpEnable = enable;
}

bool Button::getVeryLongPressEnable() { return vlpEnable; }

void Button::setClickCallback(function<void()> cbClick) { click_cb = cbClick; }

void Button::setTouchPin(int pinNum) { buttonPinNum = pinNum; }
int  Button::getTouchPin() { return buttonPinNum; }

/**
 * @brief 带防抖功能的按键检测函数，主循环中调用，识别单击，长按，超长按
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

    // 若该按键支持长按多次触发，且持续按下状态超过阈值，则触发长按事件
    if (getLongPressEnable() && lastStableState && (now - firstClickTime) > LONG_PRESS_MS)
    {
        onLongPress(); // 每次调用都触发一次长按事件
    }

    // 若该按键支持超长按，且持续按下状态超过阈值，则触发超长按事件
    if (getVeryLongPressEnable() && lastStableState && (now - firstClickTime) > VERY_LONG_PRESS_MS)
    {
        onVeryLongPress(); // 每次调用都触发一次超长按事件
    }

    // 蜂鸣器控制：按下时开启，持续100ms后关闭
    if (beepPinNum >= 0 && beepOnMillis > 0 && (now - beepOnMillis) > BEEP_DURATION_MS)
    {
        digitalWrite(beepPinNum, LOW);
        beepOnMillis = 0;
    }
}

void Button::onSingleClick()
{
    // 要先拉高蜂鸣器电平，再调用click_cb（发送MQTT），不然蜂鸣器不起振
    if (beepPinNum >= 0)
    {
        digitalWrite(beepPinNum, HIGH);
        beepOnMillis = millis();
    }
    if (click_cb) click_cb();
}

void Button::onLongPress()
{
    unsigned long now = millis();
    if ((now - lastClickTime) < REPEAT_MS) return;
    if (click_cb) click_cb();
    lastClickTime = now;
}

void Button::onVeryLongPress()
{
    if (alterPress_cb) alterPress_cb();
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