#ifndef PANEL_H
#define PANEL_H

#include <stdint.h>
#include <array>
#include <deque>
#include "U8g2lib.h"
#include "pattern.h"

using namespace std;

struct TimeSettings
{
    int  startHour[3];
    int  startMinute[3];
    int  endHour[3];
    int  endMinute[3];
    bool enabled;
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
    void     setPosition(U8G2 *u8g2, uint16_t left, uint16_t top);
    U8G2    *getU8G2();
    uint16_t getX();
    uint16_t getY();
    void     setDisplayMode(DisplayMode mode);
    void     setFlashInterval(int interval);
    void     draw(); // 统一接口，处理完显示和闪烁后，调用虚函数drawSpecific()，由派生类自己实现
    void
    draw(U8G2 *u8g2Ptr, uint16_t x, uint16_t y); // 绘制时指定坐标，仍然是调用虚函数drawSpecific()，由派生类自己实现

protected:
    void registerPattern(Pattern *newPattern);

private:
    vector<Pattern *> allPatterns;
    U8G2             *u8g2;
    uint16_t          posX;
    uint16_t          posY;
    DisplayMode       dispMode;           // 显示模式（show/hide/flash）
    bool              isVisibleNow;       // 当前是否可见
    long              lastSwitchTime;     // flash状态时的上次切换时间
    int               flashInterval;      // 闪烁间隔时间
    virtual void      drawSpecific() = 0; // 纯虚函数，由draw()调用，在派生类中实现
};

////////////////////////////////////////////////////////////
//                      CtrlPanel                         //
////////////////////////////////////////////////////////////

class ICtrlPanel : public Panel
{
public:
    virtual void setSelected(bool sel)  = 0;
    virtual bool getSelected()          = 0;
    virtual void setActive(bool active) = 0;
    virtual bool getActive()            = 0;
    virtual void switchInput()          = 0;
    virtual void inputUp()              = 0;
    virtual void inputDown()            = 0;
};

template <size_t numDigit, size_t numText>
class CtrlPanel : public ICtrlPanel
{
public:
    CtrlPanel();
    void setSelected(bool sel) override;
    bool getSelected() override;
    void setActive(bool active) override;
    bool getActive() override;
    void switchInput() override;
    void inputUp() override;
    void inputDown() override;

protected:
    array<InputDigit, numDigit>          digitFields;
    array<SelectText, numText>           textFields;
    array<Pattern *, numDigit + numText> inputFields;

private:
    bool         isSelected;
    bool         isActive;
    int          selectedField  = -1;
    virtual void inputHandler() = 0;
};

class WaterCtrl : public CtrlPanel<3, 0>
{
public:
    WaterCtrl();
    WaterSettings getSettings();
    void          setData(WaterSettings set);

private:
    void inputHandler() override;
    void drawSpecific() override;
};

class TempCtrl : public CtrlPanel<1, 0>
{
public:
    TempCtrl();
    int  getSettings();
    void setData(int set);

private:
    void inputHandler() override;
    void drawSpecific() override;
};

class TimeCtrl : public CtrlPanel<12, 1>
{
public:
    TimeCtrl();
    TimeSettings getSettings();
    void         setData(TimeSettings set);

private:
    Pattern dayPlusSign[3];
    void    inputHandler() override;
    void    drawSpecific() override;
};

////////////////////////////////////////////////////////////
//                      Indicator                         //
////////////////////////////////////////////////////////////

class PumpIndicator : public Panel
{
public:
    PumpIndicator();
    void setPumpOnOff(bool isON);
    bool getPumpOnOff();

private:
    bool          isPumpOn = false;
    int           durationMin; // 开启时长：分
    int           durationSec; // 开启时长：秒
    unsigned long startMillis = 0;
    MultiSymbol   pumpSym;
    Pattern       durationSym;
    void          calcDuration();
    void          drawSpecific() override;
};

class FlowIndicator : public Panel
{
public:
    FlowIndicator();
    void setFlow(float flow);

private:
    float waterFlow;
    void  drawSpecific() override;
};

class TempIndicator : public Panel
{
public:
    TempIndicator();
    void setTemperature(int temp);

private:
    int     temperature;
    Pattern tempSym;
    void    drawSpecific() override;
};

/**
 * @brief 128x75 温度曲线显示面板
 */
class TempCurve : public Panel
{
public:
    TempCurve();
    void setTemperature(int temp);
    void setPumpOnOff(bool isOn);

private:
    static constexpr int MAX_DATA_POINTS = 50;        // 曲线一共50个点
    static constexpr int CURVE_INTERVAL  = 30 * 1000; // 30秒刷新一个点
    int                  temperature;
    bool                 isPumpOn;
    deque<int>           tempPoints;
    deque<bool>          pumpOnPoints;
    unsigned long        lastMillis = 0;
    void                 inputHandler();
    void                 drawSpecific() override;
};

/**
 * @brief 11x23 信号强度面板
 */
class SignalIndicator : public Panel
{
public:
    SignalIndicator();
    void setStrength(int strength);

private:
    int  signalLevel;
    void drawSpecific() override;
};

class RealtimeIndicator : public Panel
{
public:
    RealtimeIndicator();
    void setRealTime(int h, int m, int s);

private:
    int  hour   = 0;
    int  minute = 0;
    int  second = 0;
    void drawSpecific() override;
};

class FPSIndicator : public Panel
{
public:
    FPSIndicator();
    float getFps();

private:
    float         screenFPS;
    unsigned long lastMillis = 0;
    int           fpsCount   = 0;
    float         fps        = 0;
    void          drawSpecific() override;
    void          refreshTick();
};

#endif