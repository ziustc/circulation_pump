#ifndef BUTTON_H
#define BUTTON_H

#include <functional>

class Button
{
public:
    Button();
    void setCallbackClick(function<void()> cbClick);
    void stateTick(bool isPinSet);

private:
    bool lastStableState;
    bool lastReading;
    unsigned long lastChangeMillis;
    unsigned long firstClickTime; // 首次按下的时间，用于判断长按，与第一次onSingleClick时间相同
    unsigned long lastClickTime;  // 持续长按时，上次触发click的时间
    function<void()> callbackClick;
    void onSingleClick();
    void onLongPress();
};

#endif
