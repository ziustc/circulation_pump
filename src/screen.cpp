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

int aaa = 0;

Screen::Screen()
{
    // 将三个主要的输入面板放到vector中，以便快速循环切换焦点，注意三个面板的顺序
    ctrlPanels.push_back(&waterPanel);
    ctrlPanels.push_back(&tempPanel);
    ctrlPanels.push_back(&timePanel);
}

void Screen::init()
{
    uint8_t csPin[]  = {42, 40, 9, 14};
    uint8_t dcPin[]  = {13, 13, 13, 13};
    uint8_t rstPin[] = {41, 39, 10, 21};

    // 初始化u8g2
    for (int i = 0; i < 4; i++)
    {
        u8g2[i] =
            new U8G2_SH1108_128X160_F_4W_HW_SPI(U8G2_R0, /* cs=*/csPin[i], /* dc=*/dcPin[i], /* reset=*/rstPin[i]);
        u8g2[i]->begin();
        u8g2[i]->enableUTF8Print();
        u8g2[i]->setBusClock(20000000);
    }

    // panel排版
    layout();

    // 数值初始化
    initIndicatorValue();
}

void Screen::layout()
{
    realtimeInd.setPosition(u8g2[0], 5, 5);
    signalInd.setPosition(u8g2[0], 105, 7);
    pumpInd.setPosition(u8g2[0], 14, 20);
    flowInd.setPosition(u8g2[0], 17, 125);
    panels[0].push_back(&realtimeInd);
    panels[0].push_back(&signalInd);
    panels[0].push_back(&pumpInd);
    panels[0].push_back(&flowInd);

    tempInd.setPosition(u8g2[1], 0, 5);
    tempCur.setPosition(u8g2[1], 0, 68);
    panels[1].push_back(&tempInd);
    panels[1].push_back(&tempCur);

    waterPanel.setPosition(u8g2[2], 0, 10);
    tempPanel.setPosition(u8g2[2], 0, 95);
    panels[2].push_back(&waterPanel);
    panels[2].push_back(&tempPanel);

    timePanel.setPosition(u8g2[3], 0, 10);
    fpsInd.setPosition(u8g2[3], 65, 155);
    panels[3].push_back(&timePanel);
    panels[3].push_back(&fpsInd);
}

void Screen::initIndicatorValue()
{
    pumpInd.setPumpOnOff(false);
    flowInd.setFlow(0);
    tempInd.setTemperature(25);
    tempCur.setTemperature(25);
    tempCur.setPumpOnOff(false);
    signalInd.setStrength(-90);
    realtimeInd.setRealTime(0, 0, 0);
}

void Screen::onShift()
{
    // 如果当前没有选中任何面板，则选中第一个面板
    if (curPanel == -1)
    {
        curPanel = 0;
        ctrlPanels[curPanel]->setSelected(true);
        return;
    }

    // 如果当前已选中某个面板，但没有激活，则轮换选中的面板
    if (!(ctrlPanels[curPanel]->getActive()))
    {
        ctrlPanels[curPanel]->setSelected(false);
        curPanel++;
        if (curPanel < ctrlPanels.size())
            // 如果下一个仍然是控制面板，则设置为选中
            ctrlPanels[curPanel]->setSelected(true);
        else // 否则退到全部未选中任何面板的状态
            curPanel = -1;
        return;
    }

    // 当前选中的面板是在激活（设置）状态，则在面板内部切换输入字段
    ctrlPanels[curPanel]->switchInput();
    return;
}

void Screen::onOK()
{
    // 如果当前没有选中任何面板，点OK无效
    if (curPanel == -1) return;

    // 如果当前面板被选中，但未激活，点OK将激活面板，进入设定状态
    if (!(ctrlPanels[curPanel]->getActive()))
    {
        ctrlPanels[curPanel]->setActive(true);
    }

    // 如果当前面板在激活状态（正在设置该面板参数），点OK将退出激活，并发送设置结果给主控
    else
    {
        ctrlPanels[curPanel]->setActive(false);
        curPanel = -1;
        exportSettings_cb(wrapupSettings()); // 回调，将所有参数返回给主控
    }
}

void Screen::onUp()
{
    // 如果当前没有选中任何面板，点UP无效
    if (curPanel == -1) return;

    // 如果当前面板只是被选中，但未激活（不在设置状态），点UP无效
    if (!(ctrlPanels[curPanel]->getActive())) return;

    // 如果当前面板激活（在设置状态）
    ctrlPanels[curPanel]->inputUp();
}

void Screen::onDown()
{
    // 如果当前没有选中任何面板，点DOWN无效
    if (curPanel == -1) return;

    // 如果当前面板只是被选中，但未激活（不在设置状态），点DOWN无效
    if (!(ctrlPanels[curPanel]->getActive())) return;

    // 如果当前面板激活（在设置状态）
    ctrlPanels[curPanel]->inputDown();
}

void Screen::onStart()
{
    // 任何状态，应可以立即启动循环泵
    bool isPumpOn = !pumpInd.getPumpOnOff();

    pumpInd.setPumpOnOff(isPumpOn);
    tempCur.setPumpOnOff(isPumpOn);
}

void Screen::updateTime(int h, int m, int s) { realtimeInd.setRealTime(h, m, s); }

void Screen::updateSignal(int strength) { signalInd.setStrength(strength); }

void Screen::updatePumpOn(bool isOn)
{
    pumpInd.setPumpOnOff(isOn);
    tempCur.setPumpOnOff(isOn);
}

void Screen::updateTempC(int temp)
{
    tempInd.setTemperature(temp);
    tempCur.setTemperature(temp);
}

void Screen::updateFlow(int flow) { flowInd.setFlow(flow); }

void Screen::setExportSettings_cb(function<void(Settings)> cb) { exportSettings_cb = cb; }

Settings Screen::wrapupSettings()
{
    Settings      ret;
    TimeSettings  timeSettings  = timePanel.getSettings();
    WaterSettings waterSettings = waterPanel.getSettings();
    int           tempSettings  = tempPanel.getSettings();

    for (int i = 0; i < 3; i++)
    {
        ret.startHour[i]   = timeSettings.startHour[i];
        ret.startMinute[i] = timeSettings.startMinute[i];
        ret.endHour[i]     = timeSettings.endHour[i];
        ret.endMinute[i]   = timeSettings.endMinute[i];
    }
    ret.waterMinSec    = waterSettings.minSec;
    ret.waterMaxSec    = waterSettings.maxSec;
    ret.pumpOnDuration = waterSettings.pumpOnDuration;
    ret.demandTemp     = tempSettings;

    return ret;
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

float Screen::draw()
{
    static unsigned long shiftMillis = 0;
    static int           shiftIndex  = 0;

    unsigned long curMillis = millis();

    for (int i = 0; i < 4; i++)
    {
        u8g2[i]->clearBuffer();
        for (int j = 0; j < panels[i].size(); j++)
        {
            panels[i][j]->draw();
        }
        u8g2[i]->sendBuffer();
    }

    return fpsInd.getFps();
}