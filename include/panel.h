#ifndef PANEL_H
#define PANEL_H

#include <stdint.h>
#include <vector>
#include "U8g2lib.h"
#include "pattern.h"

using namespace std;

struct TimeSettings
{
    int startHour[3];
    int startMinute[3];
    int endHour[3];
    int endMinute[3];
};

struct WaterSettings
{
    int minSec;
    int maxSec;
    int pumpOnDuration;
};

class Panel
{
public:
    Panel();
    virtual void setPosition(uint16_t left, uint16_t top);
    uint16_t     getX();
    uint16_t     getY();
    void         setDisplayMode(DisplayMode mode);
    void         setFlashInterval(int interval);
    void         draw(U8G2 *u8g2Ptr);       // 统一接口，处理完显示和闪烁后，调用虚函数drawCore()，由派生类自己实现

protected:
    void registerPattern(Pattern *newPattern);

private:
    vector<Pattern *> allPatterns;
    uint16_t          posX;
    uint16_t          posY;
    DisplayMode       dispMode;                        // 显示模式（show/hide/flash）
    bool              isVisibleNow;                    // 当前是否可见
    long              lastSwitchTime;                  // flash状态时的上次切换时间
    int               flashInterval;                   // 闪烁间隔时间
    virtual void      drawSpecific(U8G2 *u8g2Ptr) = 0; // 纯虚函数，由draw()调用，在派生类中实现
};

////////////////////////////////////////////////////////////
//                      CtrlPanel                         //
////////////////////////////////////////////////////////////

class CtrlPanel : public Panel
{
public:
    CtrlPanel();
    void setSelected(bool sel);
    bool getSelected();
    void setActive(bool active);
    bool getActive();
    void switchInput();
    void inputUp();
    void inputDown();

protected:
    vector<InputDigit> &getInputFields();

private:
    bool               isSelected;
    bool               isActive;
    vector<InputDigit> inputFields;
    int                selectedField = -1;
    void virtual inputHandler()      = 0;
};

class WaterCtrl : public CtrlPanel
{
public:
    WaterCtrl();
    WaterSettings getSettings();
    void          setData(WaterSettings set);

private:
    void inputHandler() override;
    void drawSpecific(U8G2 *u8g2Ptr) override;
};

class TempCtrl : public CtrlPanel
{
public:
    TempCtrl();
    int  getSettings();
    void setData(int set);

private:
    void inputHandler() override;
    void drawSpecific(U8G2 *u8g2Ptr) override;
};

class TimeCtrl : public CtrlPanel
{
public:
    TimeCtrl();
    TimeSettings getSettings();
    void         setData(TimeSettings set);

private:
    vector<Pattern> dayPlusSign;
    void            inputHandler() override;
    void            drawSpecific(U8G2 *u8g2Ptr) override;
};

////////////////////////////////////////////////////////////
//                      Indicator                         //
////////////////////////////////////////////////////////////

class PumpIndicator : public Panel
{
public:
    PumpIndicator();
    void setPumpOnOff(bool isON);
    void setWaterCurrent(float current);

private:
    bool        pumpIsOn;
    float       waterCurrent;
    MultiSymbol pumpSym;
    Pattern     currentSym;
    void        drawSpecific(U8G2 *u8g2Ptr) override;
};

#endif