#include <Arduino.h>
#include <stdio.h>
#include <U8g2lib.h>
#include "screen.h"
#include "panel.h"
#include "pattern.h"
#include "button.h"

#define TOUCH_UP    4
#define TOUCH_DOWN  5
#define TOUCH_SHIFT 6
#define TOUCH_OK    7
#define TOUCH_START 8

using namespace std;

// 让 printf 输出到 Serial
extern "C" int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        Serial.write(ptr[i]);
    }
    return len;
}

long last_millis = 0;

Screen  scr;
Button  btnUp;
Button  btnDown;
Button  btnOK;
Button  btnShift;
Button  btnStart;
Button *buttons[5] = {&btnUp, &btnDown, &btnOK, &btnShift, &btnStart};

void setup(void)
{
    Serial.begin(115200);
    Serial.println("Hello monoTFT");

    btnUp.setCallbackClick([]() { scr.onUp(); });
    btnUp.setTouchPin(TOUCH_UP);

    btnDown.setCallbackClick([]() { scr.onDown(); });
    btnDown.setTouchPin(TOUCH_DOWN);

    btnOK.setCallbackClick([]() { scr.onOK(); });
    btnOK.setTouchPin(TOUCH_OK);

    btnShift.setCallbackClick([]() { scr.onShift(); });
    btnShift.setTouchPin(TOUCH_SHIFT);

    btnStart.setCallbackClick([]() { scr.onStart(); });
    btnStart.setTouchPin(TOUCH_START);
}

void loop(void)
{
    // 整体循环频率约60Hz

    for (int i = 0; i < 5; i++)
        buttons[i]->stateTick(touchRead(buttons[i]->getTouchPin()));

    scr.draw(); // 4个屏幕刷新共约16ms

    if (millis() - last_millis >= 1000)
    {
        Serial.println("o");
        last_millis = millis();
    }
}