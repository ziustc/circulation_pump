#ifndef SCREEN_H
#define SCREEN_H

#include <vector>
#include <functional>
#include <U8g2lib.h>
#include "pattern.h"
#include "panel.h"

struct Settings
{
    int startHour[3];
    int startMinute[3];
    int endHour[3];
    int endMinute[3];
    int waterMinSec;
    int waterMaxSec;
    int pumpOnDuration;
    int demandTemp;
};

class Screen
{
public:
    Screen();
    void onShift();
    void onOK();
    void onUp();
    void onDown();
    void onStart();
    void draw();
    void updateTempC(int temp);
    void updateVolume(int volume);
    void updateTime(int h, int m, int s);
    void updateDuration(int sec);
    void importSettings(Settings set);
    void setReportSettings_cb(function<Settings()>);

private:
    vector<U8G2_SH1108_128X160_F_4W_HW_SPI> u8g2;

    PumpIndicator pumpInd;
    TimeCtrl      timePanel;
    WaterCtrl     waterPanel;
    TempCtrl      tempPanel;
    TempIndicator tempInd;
    TempCurve     tempCur;

    vector<CtrlPanel *>  panels;
    vector<Panel *>      indicators;
    int                  curPanel = -1;
    Settings             settings;
    int                  curTime;
    int                  duration;
    function<Settings()> reportSettings_cb;
    unsigned long        fpsMillis = 0;
    int                  fpsCount  = 0;
    float                fps       = 0;
    void                 wrapupSettings();
};

#endif // SCREEN_H