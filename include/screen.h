#ifndef SCREEN_H
#define SCREEN_H

#include <vector>
#include <functional>
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
    int setTemp;
};

class Screen
{
public:
    Screen();
    void onShift();
    void onOK();
    void onUp();
    void onDown();
    void draw();
    void updateTempC(int temp);
    void updateVolume(int volume);
    void updateTime(int h, int m, int s);
    void updateDuration(int sec);
    void importSettings(Settings set);
    void setCallbackSettings(function<Settings()>);

private:
    MultiSymbol pumpSymbol;
    TimeCtrl timePanel;
    WaterCtrl waterPanel;
    TempCtrl tempPanel;
    FlowIndicator flowInfo;
    DurationIndicator durationInfo;
    CurTimeIndicator timeInfo;
    StatusIndicator statusInfo;

    vector<CtrlPanel &> panels;
    Settings settings;
    int curTime;
    int duration;
    int curPanel;
    function<Settings()> callbackSettings;

    void wrapupSettings();
};

#endif // SCREEN_H