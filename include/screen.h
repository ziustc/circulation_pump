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
    void  init();
    void  onShift();
    void  onOK();
    void  onUp();
    void  onDown();
    void  onStart();
    float draw();
    void  updateTime(int h, int m, int s);
    void  updateSignal(int strength);
    void  updatePumpOn(bool isOn);
    void  updateTempC(int temp);
    void  updateFlow(int volume);
    void  importSettings(Settings set);
    void  setExportSettings_cb(function<void(Settings)> cb);

private:
    U8G2_SH1108_128X160_F_4W_HW_SPI *u8g2[4];

    PumpIndicator     pumpInd;
    FlowIndicator     flowInd;
    TimeCtrl          timePanel;
    WaterCtrl         waterPanel;
    TempCtrl          tempPanel;
    TempIndicator     tempInd;
    TempCurve         tempCur;
    SignalIndicator   signalInd;
    RealtimeIndicator realtimeInd;
    FPSIndicator      fpsInd;

    vector<CtrlPanel *>      ctrlPanels;
    vector<Panel *>          panels[4];
    int                      curPanel = -1;
    int                      curTime;
    int                      duration;
    function<void(Settings)> exportSettings_cb;
    void                     layout();
    void                     initIndicatorValue();
    Settings                 wrapupSettings();
};

#endif // SCREEN_H