#include <vector>
#include "U8g2lib.h"
#include "panel.h"
#include "pattern.h"
#include "symbols.h"
#include "common.h"

using namespace std;

/*************************************************************/
/*                           Panel                           */
/*************************************************************/

Panel::Panel()
{
    u8g2          = nullptr;
    posX          = 0;
    posY          = 0;
    dispMode      = DM_SHOW;
    isVisibleNow  = true;
    flashInterval = 500;
}

void Panel::setPosition(U8G2 *u8g2Ptr, uint16_t left, uint16_t top)
{
    u8g2 = u8g2Ptr;

    // 先将Panel下所有的Pattern同步重定义位置
    for (Pattern *eachPattern : allPatterns)
    {
        eachPattern->setU8G2(u8g2Ptr);                      // 仅设置旗下所有Pattern的u8g2
        eachPattern->movePosition(left - posX, top - posY); // 设置Pattern的位置
    }

    // 再重设本Panel的位置
    posX = left;
    posY = top;
}

U8G2 *Panel::getU8G2() { return u8g2; }

uint16_t Panel::getX() { return posX; }

uint16_t Panel::getY() { return posY; }

void Panel::setDisplayMode(DisplayMode mode)
{
    dispMode       = mode;
    lastSwitchTime = millis(); // 重置闪烁时间
    if (mode == DM_SHOW)
    {
        isVisibleNow = true;
    }
    else if (mode == DM_HIDE)
    {
        isVisibleNow = false;
    }
    else // mode==DM_FLASH
    {
        isVisibleNow = !isVisibleNow; // 先翻转，这样可以立即体现闪烁状态
    }
}

void Panel::setFlashInterval(int interval) { flashInterval = interval; }

void Panel::draw()
{
    long currentTime = millis();
    if (dispMode == DM_HIDE)
    {
        isVisibleNow = false;
    }

    else if (dispMode == DM_SHOW)
    {
        isVisibleNow = true;
    }

    else // if (dispMode == DM_FLASH)
    {
        // 如果是闪烁模式，检查是否需要切换可见状态
        if (currentTime - lastSwitchTime >= flashInterval)
        {
            isVisibleNow   = !isVisibleNow; // 切换可见状态
            lastSwitchTime = currentTime;
        }
    }

    if (isVisibleNow) drawSpecific();
}

void Panel::draw(U8G2 *u8g2Ptr, uint16_t x, uint16_t y)
{
    setPosition(u8g2Ptr, x, y);
    draw();
}

void Panel::registerPattern(Pattern *newPattern) { allPatterns.push_back(newPattern); }

/*************************************************************/
/*                         CtrlPanel                         */
/*************************************************************/

template <size_t numDigit, size_t numText>
CtrlPanel<numDigit, numText>::CtrlPanel()
{
    int idx = 0;
    for (int i = 0; i < numDigit; i++)
        inputFields[idx++] = &digitFields[i];
    for (int i = 0; i < numText; i++)
        inputFields[idx++] = &textFields[i];

    for (Pattern *field : inputFields)
        registerPattern(field);

    isSelected    = false;
    isActive      = false;
    selectedField = -1;
}

/**
 * @brief 设置面板选中状态，选中时面板闪烁，但还没进入面板编辑状态
 */
template <size_t numDigit, size_t numText>
void CtrlPanel<numDigit, numText>::setSelected(bool sel)
{
    isSelected = sel;
    if (sel)
    {
        setDisplayMode(DM_FLASH);
    }
    else
    {
        setDisplayMode(DM_SHOW);
    }
}

/**
 * @brief 返回面板选中状态
 */
template <size_t numDigit, size_t numText>
bool CtrlPanel<numDigit, numText>::getSelected()
{
    return isSelected;
}

/**
 * @brief 设置面板激活状态，激活时进入面板编辑状态，退出激活时回到面板正常显示状态
 */
template <size_t numDigit, size_t numText>
void CtrlPanel<numDigit, numText>::setActive(bool active)
{
    // 如果该面板没有可输入的字段，则不响应选中命令
    if (inputFields.empty()) return;

    // 如果面板有输入字段，先更新状态
    isActive = active;

    // 如果是进入active
    if (active)
    {
        // 进入Active则退出Selected状态
        setSelected(false);

        // 如果没有设置输入字段，则从第一个inputField开始准备输入，如果本来就在输入，则不变
        if (selectedField == -1)
        {
            selectedField = 0;
            inputFields[selectedField]->setDisplayMode(DM_FLASH);
        }
    }

    // 如果取消选中
    else
    {
        // 如果有正在编辑的InputField（一定有），则先退出编辑状态
        if (selectedField != -1)
        {
            inputFields[selectedField]->setDisplayMode(DM_SHOW);
            selectedField = -1;
        }

        // 退出Active，不需要再进入Selected状态，而是直接回到正常显示状态
    }
}

template <size_t numDigit, size_t numText>
bool CtrlPanel<numDigit, numText>::getActive()
{
    return isActive;
}

template <size_t numDigit, size_t numText>
void CtrlPanel<numDigit, numText>::switchInput()
{
    if (inputFields.empty()) return;

    if (selectedField > -1) inputFields[selectedField]->setDisplayMode(DM_SHOW);

    selectedField = (selectedField + 1) % inputFields.size();
    inputFields[selectedField]->setDisplayMode(DM_FLASH);
}

template <size_t numDigit, size_t numText>
void CtrlPanel<numDigit, numText>::inputUp()
{
    if (inputFields.empty() || selectedField == -1) return;

    inputFields[selectedField]->increase();
    inputHandler();
}

template <size_t numDigit, size_t numText>
void CtrlPanel<numDigit, numText>::inputDown()
{
    if (inputFields.empty() || selectedField == -1) return;

    inputFields[selectedField]->decrease();
    inputHandler();
}

/*************************************************************/
/*                         WaterCtrl                         */
/*************************************************************/

WaterCtrl::WaterCtrl()
{
    // 三个输入字段：开水最短时长，最长时长，水泵运行时长

    // 初始化自身位置
    setPosition(nullptr, 0, 0);

    // 初始化panel内部pattern

    digitFields[0].setFont((uint8_t *)FONT_INPUT_DIGIT);
    digitFields[1].setFont((uint8_t *)FONT_INPUT_DIGIT);
    digitFields[2].setFont((uint8_t *)FONT_INPUT_DIGIT);

    digitFields[0].setPosition(nullptr, 43, 30); // 构造函数时暂时没有u8g2用nullptr，所有坐标相对(0, 0)初始化
    digitFields[1].setPosition(nullptr, 65, 30);
    digitFields[2].setPosition(nullptr, 91, 55);

    digitFields[0].setDigitCount(1);
    digitFields[1].setDigitCount(1);
    digitFields[2].setDigitCount(1);

    digitFields[0].setLimit(1, 9);
    digitFields[1].setLimit(1, 9);
    digitFields[2].setLimit(1, 9);

    digitFields[0].setValue(2);
    digitFields[1].setValue(6);
    digitFields[2].setValue(4);
}

void WaterCtrl::drawSpecific()
{
    U8G2 *u8g2 = getU8G2();

    // panel外框
    u8g2->drawRFrame(getX(), getY(), 128, 70, 5);

    // 固定文字图案
    u8g2->setFont(FONT_CHN_16);
    u8g2->drawUTF8(getX() + 10, getY() + 30, "开水  -  秒");
    u8g2->drawUTF8(getX() + 25, getY() + 55, "水泵启动  分");

    // 输入字段
    for (auto field : inputFields)
        field->draw();
}

WaterSettings WaterCtrl::getSettings()
{
    WaterSettings ret;

    ret.minSec         = digitFields[0].getValue();
    ret.maxSec         = digitFields[1].getValue();
    ret.pumpOnDuration = digitFields[2].getValue();

    return ret;
}

void WaterCtrl::setData(WaterSettings set)
{
    digitFields[0].setValue(set.minSec);
    digitFields[1].setValue(set.maxSec);
    digitFields[2].setValue(set.pumpOnDuration);
    inputHandler();
}

void WaterCtrl::inputHandler()
{
    if (digitFields[0].getValue() > digitFields[1].getValue())
    {
        digitFields[1].setValue(digitFields[0].getValue());
    }
}

/*************************************************************/
/*                         TempCtrl                          */
/*************************************************************/

TempCtrl::TempCtrl()
{
    // 一个输入字段，水泵设定温度（该温度正负范围内启动）

    // 初始化自身位置
    setPosition(nullptr, 0, 0);

    // 设定温度显示
    digitFields[0].setFont((uint8_t *)FONT_INPUT_DIGIT);
    digitFields[0].setDigitCount(2);
    digitFields[0].setPosition(nullptr, 80, 30); // 构造函数时暂时没有u8g2用nullptr，所有坐标相对(0, 0)初始化
    digitFields[0].setLimit(TEMP_SETTING_LOWEST, TEMP_SETTING_HIGHEST);
    digitFields[0].setValue(35);
}

void TempCtrl::drawSpecific()
{
    U8G2 *u8g2 = getU8G2();

    // 外框
    u8g2->drawRFrame(getX(), getY(), 128, 45, 5);

    // 固定文字图案
    u8g2->setFont(FONT_CHN_16);
    u8g2->drawUTF8(getX() + 10, getY() + 30, "设定水温    度");

    // 输入字段
    for (auto field : inputFields)
        field->draw();
}

int TempCtrl::getSettings() { return digitFields[0].getValue(); }

void TempCtrl::setData(int set)
{
    digitFields[0].setValue(set);
    inputHandler();
}

void TempCtrl::inputHandler()
{
    if (digitFields[0].getValue() < TEMP_SETTING_LOWEST)
        digitFields[0].setValue(TEMP_SETTING_LOWEST);

    else if (digitFields[0].getValue() > TEMP_SETTING_HIGHEST)
        digitFields[0].setValue(TEMP_SETTING_HIGHEST);
}

/*************************************************************/
/*                         TimeCtrl                          */
/*************************************************************/

TimeCtrl::TimeCtrl()
{
    // 一个全局Enable字段，三组循环泵有效时段

    // 初始化自身位置
    setPosition(nullptr, 0, 0);

    // 初始化保温总开关
    static const char *textList[] = {"启用中", "禁用中"};
    textFields[0].setTextList(textList, 2);
    textFields[0].setFont((uint8_t *)FONT_CHN_16);
    textFields[0].setPosition(nullptr, 60, 30);
    textFields[0].setIndex(0);

    // 初始化保温时段字段
    for (int i = 0; i < 3; i++)
    {
        digitFields[i * 4 + 0].setFont((uint8_t *)FONT_INPUT_DIGIT);
        digitFields[i * 4 + 1].setFont((uint8_t *)FONT_INPUT_DIGIT);
        digitFields[i * 4 + 2].setFont((uint8_t *)FONT_INPUT_DIGIT);
        digitFields[i * 4 + 3].setFont((uint8_t *)FONT_INPUT_DIGIT);

        digitFields[i * 4 + 0].setPosition(nullptr, 7 + 27 * 0, 60 + i * 28);
        digitFields[i * 4 + 1].setPosition(nullptr, 7 + 27 * 1, 60 + i * 28);
        digitFields[i * 4 + 2].setPosition(nullptr, 7 + 27 * 2, 60 + i * 28);
        digitFields[i * 4 + 3].setPosition(nullptr, 7 + 27 * 3, 60 + i * 28);

        digitFields[i * 4 + 0].setLimit(0, 23);
        digitFields[i * 4 + 1].setLimit(0, 59);
        digitFields[i * 4 + 2].setLimit(0, 23);
        digitFields[i * 4 + 3].setLimit(0, 59);

        for (int j = 0; j < 4; j++)
        {
            digitFields[i * 4 + j].setDigitCount(2);
            digitFields[i * 4 + j].setValue(0);
        }
    }

    // 三个“+1天”标识
    for (int i = 0; i < 3; i++)
    {
        dayPlusSign[i].setFont((uint8_t *)FONT_NARROW_SMALL);
        dayPlusSign[i].setCode("+1");
        dayPlusSign[i].setPosition(nullptr, 112, 55 + i * 28);
        dayPlusSign[i].setDisplayMode(DM_HIDE);
        registerPattern(&dayPlusSign[i]);
    }

    // 重新安排输入顺序：先保温启用/禁用，再各时段（与父类 CtrlPanel 默认设置的“先数字后文本”相反）
    inputFields[0] = &textFields[0];
    for (int i = 0; i < 12; i++)
        inputFields[i + 1] = &digitFields[i];
}

void TimeCtrl::drawSpecific()
{
    U8G2 *u8g2 = getU8G2();

    // 外框
    u8g2->drawRFrame(getX(), getY(), 128, 130, 5);

    // 固定文字图案
    u8g2->setFont(FONT_CHN_16);
    u8g2->drawUTF8(getX() + 10, getY() + 30, "保温：");
    u8g2->setFont(FONT_INPUT_DIGIT);
    for (int i = 0; i < 3; i++)
    {
        u8g2->drawUTF8(getX() + 1 + 27 * 1, getY() + 60 + i * 28, ":");
        u8g2->drawUTF8(getX() - 1 + 27 * 2, getY() + 60 + i * 28, "-");
        u8g2->drawUTF8(getX() + 1 + 27 * 3, getY() + 60 + i * 28, ":");
    }

    // 输入字段
    for (auto fields : inputFields)
        fields->draw();

    // "+1"标识
    for (int i = 0; i < 3; i++)
        dayPlusSign[i].draw();
}

TimeSettings TimeCtrl::getSettings()
{
    TimeSettings ret;

    for (int i = 0; i < 3; i++)
    {
        ret.startHour[i]   = digitFields[i * 4 + 0].getValue();
        ret.startMinute[i] = digitFields[i * 4 + 1].getValue();
        ret.endHour[i]     = digitFields[i * 4 + 2].getValue();
        ret.endMinute[i]   = digitFields[i * 4 + 3].getValue();
    }
    ret.enabled = (textFields[0].getIndex() == 0) ? true : false;

    return ret;
}

void TimeCtrl::setData(TimeSettings set)
{
    for (int i = 0; i < 3; i++)
    {
        digitFields[i * 4].setValue(set.startHour[i]);
        digitFields[i * 4 + 1].setValue(set.startMinute[i]);
        digitFields[i * 4 + 2].setValue(set.endHour[i]);
        digitFields[i * 4 + 3].setValue(set.endMinute[i]);
    }
    textFields[0].setIndex(set.enabled ? 0 : 1);

    inputHandler();
}

void TimeCtrl::inputHandler()
{
    int beginT;
    int endT;

    for (int i = 0; i < 3; i++)
    {
        beginT = digitFields[i * 4].getValue() * 60 + digitFields[i * 4 + 1].getValue();
        endT   = digitFields[i * 4 + 2].getValue() * 60 + digitFields[i * 4 + 3].getValue();
        if (endT < beginT)
            dayPlusSign[i].setDisplayMode(DM_SHOW);
        else
            dayPlusSign[i].setDisplayMode(DM_HIDE);

        if (beginT == 0 && endT % 60 == 0)
            digitFields[i * 4 + 2].setLimit(0, 24);
        else
            digitFields[i * 4 + 2].setLimit(0, 23);
    }
}

/*************************************************************/
/*                         水泵指示器                          */
/*************************************************************/

PumpIndicator::PumpIndicator()
: pumpSym((uint8_t *)icon_cycle, 1300, 6)
, durationSym((uint8_t *)FONT_DISP_SMALL2, PT_FONT)
{
    // 无需初始化自身位置，因为C++本身会先调用基类构造函数，在Panel的构造函数中已经初始化了
    // setPosition(nullptr, 0, 0);

    // 初始化panel内部pattern
    pumpSym.setPosition(nullptr, 0, 0); // 这里是pattern相对panel左上点的相对坐标
    pumpSym.setBMPSize(100, 100);
    pumpSym.setRollingInterval(100);

    durationSym.setPosition(nullptr, 26, 58); // 这里是pattern相对panel左上点的相对坐标

    // 注册所有Pattern对象
    registerPattern(&pumpSym);
    registerPattern(&durationSym);
}

void PumpIndicator::drawSpecific()
{
    char buf[5];

    // 计算循环泵开启时长
    calcDuration();
    snprintf(buf, sizeof(buf), "%d:%02d", durationMin, durationSec);
    durationSym.setCode(buf);

    pumpSym.draw();
    durationSym.draw();
}

void PumpIndicator::setPumpOnOff(bool isOn)
{
    isPumpOn = isOn;
    pumpSym.setRollOnOff(isOn);

    if (isOn)
    {
        durationSym.setDisplayMode(DM_SHOW);
        startMillis = millis();
    }
    else
    {
        durationSym.setDisplayMode(DM_HIDE);
        startMillis = 0;
    }
}

bool PumpIndicator::getPumpOnOff() { return isPumpOn; }

void PumpIndicator::calcDuration()
{
    int dur;

    if (isPumpOn)
    {
        dur = (millis() - startMillis) / 1000;
        if (dur < 10 * 60)
        {
            durationMin = dur / 60;
            durationSec = dur % 60;
        }
        else
        {
            durationMin = 9;
            durationSec = 59;
        }
    }
    else
    {
        durationMin = 0;
        durationSec = 0;
    }
}

/*************************************************************/
/*                        流量指示器                           */
/*************************************************************/
FlowIndicator::FlowIndicator() { waterFlow = 0; }

void FlowIndicator::drawSpecific()
{
    U8G2 *u8g2 = getU8G2();
    char  buf[20];

    snprintf(buf, sizeof(buf), "%1.1f", waterFlow);
    u8g2->setFont(FONT_CHN_14);
    u8g2->drawUTF8(getX(), getY() + 16, "流量");
    u8g2->setFont(FONT_DISP_SMALL2);
    u8g2->drawStr(getX() + 31, getY() + 18, buf);
    u8g2->setFont(FONT_SMALL_ENG2);
    u8g2->drawStr(getX() + 72, getY() + 9, "L /");
    u8g2->drawStr(getX() + 72, getY() + 18, "min");
}

void FlowIndicator::setFlow(float flow) { waterFlow = flow; }

/*************************************************************/
/*                        温度指示器                           */
/*************************************************************/

TempIndicator::TempIndicator()
: tempSym((uint8_t *)FONT_DISP_LARGE, PT_FONT)
{
    temperature = 25;
    tempSym.setPosition(nullptr, 44, 50);
    registerPattern(&tempSym);
}

void TempIndicator::setTemperature(int temp) { temperature = temp; }

void TempIndicator::drawSpecific()
{
    U8G2 *u8g2 = getU8G2();
    char  buf[5];

    u8g2->drawXBMP(getX() + 15, getY() + 18, 20, 32, icon_temp);
    u8g2->setFont(FONT_DISP_SMALL);
    u8g2->drawStr(getX() + 97,
                  getY() + 35,
                  "\xb0"
                  "C");

    snprintf(buf, sizeof(buf), "%d", temperature);
    tempSym.setCode(buf);
    tempSym.draw();
}

/*************************************************************/
/*                        温度曲线                            */
/*************************************************************/

TempCurve::TempCurve() { setPosition(nullptr, 0, 0); }

void TempCurve::setTemperature(int temp) { temperature = temp; }

void TempCurve::setPumpOnOff(bool isOn) { isPumpOn = isOn; }

void TempCurve::inputHandler()
{
    unsigned long now = millis();

    if (now - lastMillis >= CURVE_INTERVAL)
    {
        lastMillis = now;
        if (tempPoints.size() >= MAX_DATA_POINTS)
        {
            tempPoints.pop_back();
            pumpOnPoints.pop_back();
        }
        tempPoints.push_front(temperature);
        pumpOnPoints.push_front(isPumpOn);
    }
}

void TempCurve::drawSpecific()
{
    U8G2 *u8g2 = getU8G2();

    inputHandler();

    // temp坐标轴
    u8g2->drawLine(getX() + 18, getY() + 50, getX() + 123, getY() + 50); // X轴
    u8g2->drawLine(getX() + 18, getY() + 50, getX() + 18, getY() + 0);   // Y轴
    u8g2->drawLine(getX() + 18, getY() + 44, getX() + 20, getY() + 44);  // 刻度线
    u8g2->drawLine(getX() + 18, getY() + 4, getX() + 20, getY() + 4);    // 刻度线
    u8g2->setFont(FONT_SMALL_ENG);
    u8g2->drawStr(getX() + 0, getY() + 44 + 4, "20");
    u8g2->drawStr(getX() + 0, getY() + 4 + 4, "40");

    // pumpOn坐标轴
    u8g2->drawLine(getX() + 18, getY() + 75, getX() + 123, getY() + 75); // X轴
    u8g2->drawLine(getX() + 18, getY() + 75, getX() + 18, getY() + 58);  // Y轴
    u8g2->setFont(FONT_SMALL_ENG);
    u8g2->drawStr(getX() + 0, getY() + 70, "ON");

    if (tempPoints.size() < 2) return;

    // 曲线
    for (int i = 0; i < tempPoints.size() - 1; i++)
    {
        // temp曲线
        if (tempPoints[i] < 17) tempPoints[i] = 17;
        if (tempPoints[i] > 40) tempPoints[i] = 40;
        if (tempPoints[i + 1] < 17) tempPoints[i + 1] = 17;
        if (tempPoints[i + 1] > 40) tempPoints[i + 1] = 40;

        u8g2->drawLine(getX() + 120 - i * 2,
                       getY() + 44 - (tempPoints[i] - 20) * 2,
                       getX() + 120 - (i + 1) * 2,
                       getY() + 44 - (tempPoints[i + 1] - 20) * 2);

        // pumpOn曲线
        if (pumpOnPoints[i]) u8g2->drawBox(getX() + 120 - i * 2, getY() + 60, 2, 15);
    }
}

/*************************************************************/
/*                      信号强度指示器                         */
/*************************************************************/

SignalIndicator::SignalIndicator()
{
    setPosition(nullptr, 0, 0);
    signalLevel = 1;
}

void SignalIndicator::setStrength(int strength)
{
    if (strength > -50)
        signalLevel = 4;
    else if (strength > -65)
        signalLevel = 3;
    else if (strength > -80)
        signalLevel = 2;
    else
        signalLevel = 1;
}

void SignalIndicator::drawSpecific()
{
    U8G2 *u8g2 = getU8G2();
    u8g2->drawXBMP(getX(), getY(), 18, 9, icon_signal[signalLevel - 1]);
}

/*************************************************************/
/*                      真实时间指示器                         */
/*************************************************************/

RealtimeIndicator::RealtimeIndicator() { setPosition(nullptr, 0, 0); }

void RealtimeIndicator::setRealTime(int h, int m, int s)
{
    hour   = h;
    minute = m;
    second = s;
}

void RealtimeIndicator::drawSpecific()
{
    U8G2 *u8g2 = getU8G2();

    char buf[6] = {0};

    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);

    u8g2->setFont(FONT_MID_ENG);
    u8g2->drawUTF8(getX(), getY() + 11, buf);
}

/*************************************************************/
/*                         FPS指示器                          */
/*************************************************************/

FPSIndicator::FPSIndicator()
{
    setPosition(nullptr, 0, 0);
    lastMillis = 0;
    fps        = 0;
}

void FPSIndicator::drawSpecific()
{
    U8G2 *u8g2 = getU8G2();
    char  buf[20];

    refreshTick();

    snprintf(buf, sizeof(buf), "fps=%.1f", fps);
    u8g2->setFont(FONT_MID_ENG);
    u8g2->drawStr(getX(), getY(), buf);
}

void FPSIndicator::refreshTick()
{
    unsigned long curMillis = millis();

    fpsCount++;
    if (curMillis - lastMillis >= 500)
    {
        fps        = (float)fpsCount * 1000 / (curMillis - lastMillis);
        lastMillis = curMillis;
        fpsCount   = 0;
    }
}

float FPSIndicator::getFps() { return fps; }
