#include <vector>
#include <functional>
#include "screen.h"
#include "panel.h"
#include "pattern.h"
#include "display_driver.h"

using namespace std;

Screen::Screen()
{
    // 循环泵动态符号
    pumpSymbol.setPosition(5, 84);
    pumpSymbol.setPatternSet(u8g2_font_wqy16_t_gb2312b);
    pumpSymbol.setDisplayMode(DM_SHOW);
    pumpSymbol.setRollingInterval(100);
    pumpSymbol.setSymbolCount(5);
    pumpSymbol.setSymbolIndex(0);

    // 三个主控面板定位
    timePanel.setPosition(80, 5);
    waterPanel.setPosition(80, 123);
    tempPanel.setPosition(390, 5);

    // 将三个主要的输入面板放到vector中，以便快速循环切换焦点，注意三个面板的顺序
    panels.push_back(timePanel);
    panels.push_back(waterPanel);
    panels.push_back(tempPanel);

    // 其他面板定位
    flowInfo.setPosition(5, 5);
    durationInfo.setPosition(700, 5);
    timeInfo.setPosition(700, 55);
    statusInfo.setPosition(700, 105);

    // 初始状态
    curPanel = -1;
}

void Screen::onShift()
{
    // 如果当前没有选中任何面板，则选中第一个面板
    if (curPanel == -1)
    {
        curPanel++;
        panels[curPanel].setSelected(true);
        return;
    }

    // 如果当前选中的面板是在激活（设置）状态，则在面板内部切换输入字段
    if (panels[curPanel].getActive())
    {
        panels[curPanel].switchInput();
        return;
    }

    // 如果面板只是选中没有激活，则轮换选中的面板
    panels[curPanel++].setSelected(false);
    if (curPanel >= 3)
        curPanel = -1;
    else
        panels[curPanel].setSelected(true);
}

void Screen::onOK()
{
    // 如果当前没有选中任何面板，点OK无效
    if (curPanel == -1)
        return;

    // 如果当前面板在激活状态（正在设置该面板参数），点OK将退出激活，退出设定状态
    if (panels[curPanel].getActive())
    {
        panels[curPanel].setActive(false);
        curPanel = -1;
        wrapupSettings();
        callbackSettings(); // 回调，将所有参数返回给主控
    }

    // 如果当前面板被选中，但未激活，点OK将激活面板，进入设定状态
    else
    {
        panels[curPanel].setSelected(false);
        panels[curPanel].setActive(true);
    }
}

void Screen::onUp()
{
    // 如果当前没有选中任何面板，点UP无效
    if (curPanel == -1)
        return;

    // 如果当前面板只是被选中，但未激活（不在设置状态），点UP无效
    if (!(panels[curPanel].getActive()))
        return;

    // 如果当前面板激活（在设置状态）
    panels[curPanel].inputUp();
}

void Screen::onDown()
{
    // 如果当前没有选中任何面板，点UP无效
    if (curPanel == -1)
        return;

    // 如果当前面板只是被选中，但未激活（不在设置状态），点UP无效
    if (!(panels[curPanel].getActive()))
        return;

    // 如果当前面板激活（在设置状态）
    panels[curPanel].inputDown();
}

void Screen::updateTempC(int temp)
{
    tempPanel.setCurTemp(temp);
}

void Screen::updateVolume(int volume)
{
    flowInfo.setData(volume);
}

void Screen::updateTime(int h, int m, int s)
{
    timeInfo.setData(h, m, s);
}

void Screen::setCallbackSettings(function<Settings()> cb)
{
    callbackSettings = cb;
}

void Screen::wrapupSettings()
{
    TimeSettings timeSettings = timePanel.getSettings();
    WaterSettings waterSettings = waterPanel.getSettings();
    int tempSettings = tempPanel.getSettings();

    for (int i = 0; i < 3; i++)
    {
        settings.startHour[i] = timeSettings.startHour[i];
        settings.startMinute[i] = timeSettings.startMinute[i];
        settings.endHour[i] = timeSettings.endHour[i];
        settings.endMinute[i] = timeSettings.endMinute[i];
    }
    settings.waterMinSec = waterSettings.minSec;
    settings.waterMaxSec = waterSettings.maxSec;
    settings.pumpOnDuration = waterSettings.pumpOnDuration;
    settings.setTemp = tempSettings;
}

void Screen::importSettings(Settings set)
{
    TimeSettings timeSettings;
    WaterSettings waterSettings;
    int tempSettings;

    for (int i = 0; i < 3; i++)
    {
        timeSettings.startHour[i] = set.startHour[i];
        timeSettings.startMinute[i] = set.startMinute[i];
        timeSettings.endHour[i] = set.endHour[i];
        timeSettings.endMinute[i] = set.endMinute[i];
    }
    waterSettings.minSec = set.waterMinSec;
    waterSettings.maxSec = set.waterMaxSec;
    waterSettings.pumpOnDuration = set.pumpOnDuration;
    tempSettings = set.setTemp;

    timePanel.setData(timeSettings);
    waterPanel.setData(waterSettings);
    tempPanel.setData(tempSettings);
}

void Screen::draw()
{
    pumpSymbol.draw();

    timePanel.draw();
    waterPanel.draw();
    tempPanel.draw();

    flowInfo.draw();
    durationInfo.draw();
    timeInfo.draw();
    statusInfo.draw();
}