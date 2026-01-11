#ifndef BUTTON_H
#define BUTTON_H

#include <functional>

#define TP_MAX_ARR   32
#define TP_THRESHOLD 1.05f

using namespace std;

class Button
{
public:
    Button();
    void setCallbackClick(function<void()> cbClick);
    void setTouchPin(int pinNum);
    int  getTouchPin();
    void stateTick(int pinValue);

private:
    int              buttonPinNum;
    int              pinValues[32] = {0};
    int              pinSum        = 0;
    int              tpIndex       = 0;
    bool             tpCalibrated  = false;
    bool             lastStableState;
    bool             lastReading;
    unsigned long    lastChangeMillis;
    unsigned long    firstClickTime; // 首次按下的时间，用于判断长按，与第一次onSingleClick时间相同
    unsigned long    lastClickTime;  // 持续长按时，上次触发click的时间
    function<void()> callbackClick;
    void             onSingleClick();
    void             onLongPress();
    bool             readTP(int pinValue); // 根据TouchPin的数值，识别按键按下状态
};

#endif
