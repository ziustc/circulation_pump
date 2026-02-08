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
    void  updateSignal(int strength);
    void  updateTempC(int temp);
    void  updateVolume(int volume);
    void  updateTime(int h, int m, int s);
    void  updateDuration(int sec);
    void  importSettings(Settings set);
    void  setReportSettings_cb(function<Settings()>);

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

    vector<CtrlPanel *> ctrlPanels;
    vector<Panel *>     panels[4];
    int                 curPanel = -1;
    Settings            settings;
    int                 curTime;
    int                 duration;
    // int                  countOfDraw = 0; // 调用draw()的总次数
    function<Settings()> reportSettings_cb;
    void                 layout();
    void                 wrapupSettings();
};

#endif // SCREEN_H