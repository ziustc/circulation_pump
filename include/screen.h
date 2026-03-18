#ifndef SCREEN_H
#define SCREEN_H

#include <vector>
#include <functional>
#include <U8g2lib.h>
#include "pattern.h"
#include "panel.h"
#include "common.h"

class Screen
{
public:
    Screen();
    void       init();
    void       onShift();
    void       onOK();
    void       onUp();
    void       onDown();
    void       onStart();
    float      draw();
    void       updateTime(int h, int m, int s);
    void       updateSignal(int strength);
    void       updatePumpOn(bool isOn);
    void       updateTempC(int temp);
    void       updateFlow(int volume);
    void       importSettings(Settings_t set);
    Settings_t exportSettings();
    void       setExportSettings_cb(function<void(Settings_t)> cb);

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

    vector<CtrlPanel *>        ctrlPanels;
    vector<Panel *>            panels[4];
    int                        curPanel = -1;
    int                        curTime;
    int                        duration;
    function<void(Settings_t)> exportSettings_cb;
    void                       layout();
    void                       initIndicatorValue();
    Settings_t                 wrapupSettings();
};

#endif // SCREEN_H