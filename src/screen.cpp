#include <stdio.h>
#include <vector>
#include <functional>
#include <U8g2lib.h>
#include "screen.h"
#include "panel.h"
#include "pattern.h"
#include "symbols.h"
#include "common.h"

using namespace std;

Screen::Screen()
{
    Serial.println("Screen initializing...");
    u8g2.emplace_back(U8G2_R0, /* cs=*/42, /* dc=*/13, /* reset=*/41);
    u8g2.emplace_back(U8G2_R0, /* cs=*/40, /* dc=*/13, /* reset=*/39);
    u8g2.emplace_back(U8G2_R0, /* cs=*/9, /* dc=*/13, /* reset=*/10);
    u8g2.emplace_back(U8G2_R0, /* cs=*/14, /* dc=*/13, /* reset=*/21);

    for (int i = 0; i < 4; i++)
    {
        u8g2[i].begin();
        u8g2[i].enableUTF8Print();
        u8g2[i].setBusClock(20000000);
    }

    // 泵循环标
    pumpInd.setPosition(0, 0);
    pumpInd.setPumpOnOff(true);

    // 温度标识
    tempInd.setPosition(0, 5);

    // 温度曲线
    tempCur.setPosition(0, 70);

    // 三个主控面板定位
    timePanel.setPosition(0, 0);
    waterPanel.setPosition(0, 0);
    tempPanel.setPosition(0, 80);

    // 将三个主要的输入面板放到vector中，以便快速循环切换焦点，注意三个面板的顺序
    panels.push_back(&timePanel);
    panels.push_back(&waterPanel);
    panels.push_back(&tempPanel);

    // 将所有其他Indicator放到vector中
    indicators.push_back(&pumpInd);
    indicators.push_back(&tempInd);

    // 初始状态
    curPanel = -1;
}

void Screen::onShift()
{
    // 如果当前没有选中任何面板，则选中第一个面板
    if (curPanel == -1)
    {
        curPanel = 0;
        panels[curPanel]->setSelected(true);
        return;
    }

    // 如果当前已选中某个面板，但没有激活，则轮换选中的面板
    if (!(panels[curPanel]->getActive()))
    {
        panels[curPanel]->setSelected(false);
        curPanel = (curPanel + 1) % panels.size();
        panels[curPanel]->setSelected(true);
        return;
    }

    // 当前选中的面板是在激活（设置）状态，则在面板内部切换输入字段
    panels[curPanel]->switchInput();
    return;
}

void Screen::onOK()
{
    // 如果当前没有选中任何面板，点OK无效
    if (curPanel == -1) return;

    // 如果当前面板被选中，但未激活，点OK将激活面板，进入设定状态
    if (!(panels[curPanel]->getActive()))
    {
        panels[curPanel]->setActive(true);
    }

    // 如果当前面板在激活状态（正在设置该面板参数），点OK将退出激活，并发送设置结果给主控
    else
    {
        panels[curPanel]->setActive(false);
        curPanel = -1;
        wrapupSettings();
        reportSettings_cb(); // 回调，将所有参数返回给主控
    }
}

void Screen::onUp()
{
    // 如果当前没有选中任何面板，点UP无效
    if (curPanel == -1) return;

    // 如果当前面板只是被选中，但未激活（不在设置状态），点UP无效
    if (!(panels[curPanel]->getActive())) return;

    // 如果当前面板激活（在设置状态）
    panels[curPanel]->inputUp();
}

void Screen::onDown()
{
    // 如果当前没有选中任何面板，点DOWN无效
    if (curPanel == -1) return;

    // 如果当前面板只是被选中，但未激活（不在设置状态），点DOWN无效
    if (!(panels[curPanel]->getActive())) return;

    // 如果当前面板激活（在设置状态）
    panels[curPanel]->inputDown();
}

void Screen::onStart()
{
    // 任何状态，应可以立即启动循环泵
}

void Screen::updateTempC(int temp) 
{ 
    tempPanel.setData(temp); 
    tempInd.setTemperature(temp);
    
}

// void Screen::updateVolume(int volume) { flowInfo.setData(volume); }

// void Screen::updateTime(int h, int m, int s) { timeInfo.setData(h, m, s); }

void Screen::setReportSettings_cb(function<Settings()> cb) { reportSettings_cb = cb; }

void Screen::wrapupSettings()
{
    TimeSettings  timeSettings  = timePanel.getSettings();
    WaterSettings waterSettings = waterPanel.getSettings();
    int           tempSettings  = tempPanel.getSettings();

    for (int i = 0; i < 3; i++)
    {
        settings.startHour[i]   = timeSettings.startHour[i];
        settings.startMinute[i] = timeSettings.startMinute[i];
        settings.endHour[i]     = timeSettings.endHour[i];
        settings.endMinute[i]   = timeSettings.endMinute[i];
    }
    settings.waterMinSec    = waterSettings.minSec;
    settings.waterMaxSec    = waterSettings.maxSec;
    settings.pumpOnDuration = waterSettings.pumpOnDuration;
    settings.demandTemp     = tempSettings;
}

void Screen::importSettings(Settings set)
{
    TimeSettings  timeSettings;
    WaterSettings waterSettings;
    int           tempSettings;

    for (int i = 0; i < 3; i++)
    {
        timeSettings.startHour[i]   = set.startHour[i];
        timeSettings.startMinute[i] = set.startMinute[i];
        timeSettings.endHour[i]     = set.endHour[i];
        timeSettings.endMinute[i]   = set.endMinute[i];
    }
    waterSettings.minSec         = set.waterMinSec;
    waterSettings.maxSec         = set.waterMaxSec;
    waterSettings.pumpOnDuration = set.pumpOnDuration;
    tempSettings                 = set.demandTemp;

    timePanel.setData(timeSettings);
    waterPanel.setData(waterSettings);
    tempPanel.setData(tempSettings);
}

void Screen::draw()
{
    static unsigned long shiftMillis = 0;
    static int           shiftIndex  = 0;

    static unsigned long buttonMillis  = 0;
    static bool          buttonPressed = false;

    unsigned long curMillis = millis();
    char          buf[20];

    // 计算FPS
    fpsCount++;
    if (curMillis - fpsMillis >= 500)
    {
        fps       = (float)fpsCount * 1000 / (curMillis - fpsMillis);
        fpsMillis = curMillis;
        fpsCount  = 0;
    }

    // 切换屏幕
    if (curMillis - shiftMillis >= 5000)
    {
        shiftIndex  = (shiftIndex + 1) % 4;
        shiftMillis = curMillis;
    }

    // 按键
    if (curMillis - buttonMillis >= 200)
    {
        buttonPressed = true;
        buttonMillis  = curMillis;
    }

    if (buttonPressed)
    {
        for (auto pnl : panels)
            pnl->inputUp();
        buttonPressed = false;
    }

    // ============================================================//
    //                           多屏显示方案                        //
    // ============================================================//

    // 分屏显示
    u8g2[0].clearBuffer();
    pumpInd.draw(&u8g2[0]);
    // 显示FPS
    snprintf(buf, sizeof(buf), "fps = %.1f", fps);
    u8g2[0].setFont(FONT_SMALL_ENG);
    u8g2[0].drawStr(1, 159, buf);
    u8g2[0].sendBuffer();

    u8g2[1].clearBuffer();
    tempInd.draw(&u8g2[1]);
    tempCur.draw(&u8g2[1]);
    u8g2[1].sendBuffer();

    u8g2[2].clearBuffer();
    waterPanel.draw(&u8g2[2]);
    tempPanel.draw(&u8g2[2]);
    u8g2[2].sendBuffer();

    u8g2[3].clearBuffer();
    timePanel.draw(&u8g2[3]);
    u8g2[3].sendBuffer();

    // ============================================================//
    //                           单屏轮流显示                        //
    // ============================================================//
    // u8g2[0].clearBuffer();

    // // // 显示FPS
    // snprintf(buf, sizeof(buf), "fps=%.1f", fps);
    // u8g2[0].setFont(FONT_SMALL_ENG);
    // u8g2[0].drawStr(1, 159, buf);

    // if (shiftIndex == 0)
    // {
    //     pumpInd.draw(&u8g2[0]);
    // }
    // else if (shiftIndex == 1)
    // {
    //     tempInd.draw(&u8g2[0]);
    //     tempCur.draw(&u8g2[0]);
    // }
    // else if (shiftIndex == 2)
    // {
    //     waterPanel.draw(&u8g2[0]);
    //     tempPanel.draw(&u8g2[0]);
    // }
    // else // shiftIndex == 3
    // {
    //     timePanel.draw(&u8g2[0]);
    // }

    // u8g2[0].sendBuffer();
}