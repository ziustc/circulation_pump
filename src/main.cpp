#include <Arduino.h>
#include <stdio.h>
#include <U8g2lib.h>
#include "screen.h"
#include "panel.h"
#include "pattern.h"
#include "button.h"

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

Screen scr;
Button btnUp;
Button btnDown;
Button btnOK;
Button btnShift;
Button btnStart;

void setup(void)
{
    Serial.begin(115200);
    Serial.println("Hello monoTFT");

    btnUp.setCallbackClick([]() { scr.onUp(); });
    btnDown.setCallbackClick([]() { scr.onDown(); });
    btnOK.setCallbackClick([]() { scr.onOK(); });
    btnShift.setCallbackClick([]() { scr.onShift(); });
}

void loop(void)
{
    scr.draw();
    // 做个小测试
}