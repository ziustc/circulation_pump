#include <Arduino.h>
#include <stdio.h>
#include <U8g2lib.h>
#include "screen.h"
#include "panel.h"
#include "pattern.h"

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

void setup(void)
{
    Serial.begin(115200);
    Serial.println("Hello monoTFT");

    // 测试printf
}

void loop(void)
{
    scr.draw();
    // 做个小测试
}