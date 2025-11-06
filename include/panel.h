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
    void         setU8G2(U8G2 *u8g2Ptr);
    void         setPosition(uint16_t left, uint16_t top);
    virtual void draw() = 0;

protected:
    U8G2    *u8g2;
    uint16_t posX;
    uint16_t posY;
};

class CtrlPanel : public Panel
{
public:
    CtrlPanel();
    void setSelected(bool sel);
    void setActive(bool active);
    bool getActive();
    void switchInput();
    void inputUp();
    void inputDown();

protected:
    Pattern            &getPanelLogo();
    vector<InputDigit> &getInputFields();

private:
    bool                         isSelected;
    bool                         isActive;
    Pattern                      PanelLogo;
    vector<InputDigit>           inputFields;
    vector<InputDigit>::iterator selectedField;
    void virtual inputHandler() = 0;
};

class WaterCtrl : public CtrlPanel
{
public:
    WaterCtrl();
    void draw() override;
    WaterSettings getSettings();
    void          setData(WaterSettings set);

private:
    void inputHandler() override;
};

class TempCtrl : public CtrlPanel
{
public:
    TempCtrl();
    void draw() override;
    void setCurTemp(int temperature);
    int  getSettings();
    void setData(int set);

private:
    Pattern curTemp;
    int     tempC;
    void    inputHandler() override;
};

class TimeCtrl : public CtrlPanel
{
public:
    TimeCtrl();
    void         draw() override;
    TimeSettings getSettings();
    void         setData(TimeSettings set);

private:
    vector<Pattern> dayPlusSign;
    void            inputHandler() override;
};

class Indicator : public Panel
{
public:
    Indicator() { };
    virtual void draw() = 0;
};

class FlowIndicator : public Indicator
{
public:
    FlowIndicator() { };
    void setData(int flow);
    void draw();

private:
    int flow;
};

class DurationIndicator : public Panel
{
public:
    DurationIndicator() { };
    void setData(int sec);
    void draw();

private:
    int duration;
};

class CurTimeIndicator : public Panel
{
public:
    CurTimeIndicator() { };
    void setData(int h, int m, int s);
    void draw();

private:
    int hh, mm, ss;
};

class StatusIndicator : public Panel
{
public:
    StatusIndicator() { };
    void setWifi(bool connected);
    void draw();

private:
    bool wifiConnected;
};

#endif