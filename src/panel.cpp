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
    posX          = 0;
    posY          = 0;
    dispMode      = DM_SHOW;
    isVisibleNow  = true;
    flashInterval = 500;
}

void Panel::setPosition(uint16_t left, uint16_t top)
{
    // 先将Panel下所有的Pattern同步重定义位置
    for (Pattern *patternPtr : allPatterns)
        patternPtr->movePosition(left - posX, top - posY);

    // 再重设本Panel的位置
    posX = left;
    posY = top;
}

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

void Panel::draw(U8G2 *u8g2Ptr)
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

    if (isVisibleNow) drawSpecific(u8g2Ptr);
}

void Panel::registerPattern(Pattern *newPattern) { allPatterns.push_back(newPattern); }
/*************************************************************/
/*                         CtrlPanel                         */
/*************************************************************/

CtrlPanel::CtrlPanel()
{
    isSelected    = false;
    isActive      = false;
    selectedField = -1;
}

/**
 * @brief 设置面板选中状态，选中时面板闪烁，但还没进入面板编辑状态
 */
void CtrlPanel::setSelected(bool sel)
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
bool CtrlPanel::getSelected() { return isSelected; }

/**
 * @brief 设置面板激活状态，激活时进入面板编辑状态，退出激活时回到面板正常显示状态
 */
void CtrlPanel::setActive(bool active)
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
            inputFields[selectedField].setDisplayMode(DM_FLASH);
        }
    }

    // 如果取消选中
    else
    {
        // 如果有正在编辑的InputField（一定有），则先退出编辑状态
        if (selectedField != -1)
        {
            inputFields[selectedField].setDisplayMode(DM_SHOW);
            selectedField = -1;
        }

        // 退出Active，不需要再进入Selected状态，而是直接回到正常显示状态
    }
}

bool CtrlPanel::getActive() { return isActive; }

void CtrlPanel::switchInput()
{
    if (inputFields.empty()) return;

    if (selectedField > -1) inputFields[selectedField].setDisplayMode(DM_SHOW);

    selectedField = (selectedField + 1) % inputFields.size();
    inputFields[selectedField].setDisplayMode(DM_FLASH);
}

void CtrlPanel::inputUp()
{
    if (inputFields.empty() || selectedField == -1) return;

    inputFields[selectedField].increase();
    inputHandler();
}

void CtrlPanel::inputDown()
{
    if (inputFields.empty() || selectedField == -1) return;

    inputFields[selectedField].decrease();
    inputHandler();
}

vector<InputDigit> &CtrlPanel::getInputFields() { return inputFields; }

/*************************************************************/
/*                         WaterCtrl                         */
/*************************************************************/

WaterCtrl::WaterCtrl()
{
    // 三个输入字段：开水最短时长，最长时长，水泵运行时长
    vector<InputDigit> &inputFields = getInputFields();

    inputFields.emplace_back(InputDigit((uint8_t *)FONT_INPUT_DIGIT));
    inputFields.emplace_back(InputDigit((uint8_t *)FONT_INPUT_DIGIT));
    inputFields.emplace_back(InputDigit((uint8_t *)FONT_INPUT_DIGIT));

    inputFields[0].setPosition(getX() + 43, getY() + 30);
    inputFields[1].setPosition(getX() + 65, getY() + 30);
    inputFields[2].setPosition(getX() + 91, getY() + 55);

    inputFields[0].setDigitCount(1);
    inputFields[1].setDigitCount(1);
    inputFields[2].setDigitCount(1);

    inputFields[0].setLimit(1, 9);
    inputFields[1].setLimit(1, 9);
    inputFields[2].setLimit(1, 9);

    inputFields[0].setValue(2);
    inputFields[1].setValue(6);
    inputFields[2].setValue(4);

    // 注册所有Pattern对象
    registerPattern(&inputFields[0]);
    registerPattern(&inputFields[1]);
    registerPattern(&inputFields[2]);

    switchInput();
    switchInput();
}

void WaterCtrl::drawSpecific(U8G2 *u8g2Ptr)
{
    vector<InputDigit> inputFields = getInputFields();

    // panel外框
    u8g2Ptr->drawRFrame(getX(), getY(), 128, 70, 5);

    // 固定文字图案
    u8g2Ptr->setFont(FONT_CHN_16);
    u8g2Ptr->drawUTF8(getX() + 10, getY() + 30, "开水  -  秒");
    u8g2Ptr->drawUTF8(getX() + 25, getY() + 55, "水泵启动  分");

    // inputFields[0].draw(u8g2Ptr, getX() + 43, getY() + 30);
    // inputFields[1].draw(u8g2Ptr, getX() + 65, getY() + 30);
    // inputFields[2].draw(u8g2Ptr, getX() + 91, getY() + 55);

    // 输入字段
    for (auto &inputFields : getInputFields())
        inputFields.draw(u8g2Ptr);
}

WaterSettings WaterCtrl::getSettings()
{
    WaterSettings       ret;
    vector<InputDigit> &inputFields = getInputFields();

    ret.minSec         = inputFields[0].getValue();
    ret.maxSec         = inputFields[1].getValue();
    ret.pumpOnDuration = inputFields[2].getValue();

    return ret;
}

void WaterCtrl::setData(WaterSettings set)
{
    vector<InputDigit> &inputFields = getInputFields();

    inputFields[0].setValue(set.minSec);
    inputFields[1].setValue(set.maxSec);
    inputFields[2].setValue(set.pumpOnDuration);
}

void WaterCtrl::inputHandler()
{
    vector<InputDigit> &inputFields = getInputFields();
    if (inputFields[0].getValue() > inputFields[1].getValue())
    {
        inputFields[1].setValue(inputFields[0].getValue());
    }
}

/*************************************************************/
/*                         TempCtrl                          */
/*************************************************************/

TempCtrl::TempCtrl()
{
    // 一个输入字段，水泵设定温度（该温度正负范围内启动）
    vector<InputDigit> &inputFields = getInputFields();
    inputFields.emplace_back(InputDigit((uint8_t *)FONT_INPUT_DIGIT));

    // 设定温度显示
    inputFields[0].setDigitCount(2);
    inputFields[0].setPosition(80, 30);
    inputFields[0].setLimit(25, 45);
    inputFields[0].setValue(35);

    // 注册所有Pattern对象
    registerPattern(&inputFields[0]);

    switchInput();
}

void TempCtrl::drawSpecific(U8G2 *u8g2Ptr)
{
    // 外框
    u8g2Ptr->drawRFrame(getX(), getY(), 128, 45, 5);

    // 固定文字图案
    u8g2Ptr->setFont(FONT_CHN_16);
    u8g2Ptr->drawUTF8(getX() + 10, getY() + 30, "设定水温    度");

    // 输入字段
    for (auto &inputFields : getInputFields())
        inputFields.draw(u8g2Ptr);
}

int TempCtrl::getSettings() { return getInputFields()[0].getValue(); }

void TempCtrl::setData(int set)
{
    vector<InputDigit> &inputFields = getInputFields();
    inputFields[0].setValue(set);
}

void TempCtrl::inputHandler() { }

/*************************************************************/
/*                         TimeCtrl                          */
/*************************************************************/

TimeCtrl::TimeCtrl()
{
    // 三组循环泵有效时段
    vector<InputDigit> &inputFields = getInputFields();
    for (int i = 0; i < 3; i++)
    {
        inputFields.emplace_back(InputDigit((uint8_t *)FONT_INPUT_DIGIT));
        inputFields.emplace_back(InputDigit((uint8_t *)FONT_INPUT_DIGIT));
        inputFields.emplace_back(InputDigit((uint8_t *)FONT_INPUT_DIGIT));
        inputFields.emplace_back(InputDigit((uint8_t *)FONT_INPUT_DIGIT));

        inputFields[i * 4 + 0].setPosition(getX() + 7 + 27 * 0, getY() + 60 + i * 25);
        inputFields[i * 4 + 1].setPosition(getX() + 7 + 27 * 1, getY() + 60 + i * 25);
        inputFields[i * 4 + 2].setPosition(getX() + 7 + 27 * 2, getY() + 60 + i * 25);
        inputFields[i * 4 + 3].setPosition(getX() + 7 + 27 * 3, getY() + 60 + i * 25);

        inputFields[i * 4 + 0].setLimit(0, 23);
        inputFields[i * 4 + 1].setLimit(0, 59);
        inputFields[i * 4 + 2].setLimit(0, 23);
        inputFields[i * 4 + 3].setLimit(0, 59);

        for (int j = 0; j < 4; j++)
        {
            inputFields[i * 4 + j].setDigitCount(2);
            inputFields[i * 4 + j].setValue(0);
        }
    }

    // 三个“+1天”标识
    for (int i = 0; i < 3; i++)
    {
        dayPlusSign.emplace_back(Pattern((uint8_t *)FONT_NARROW_SMALL, PT_FONT));

        dayPlusSign[i].setCode("+1");
        dayPlusSign[i].setPosition(getX() + 112, getY() + 55 + i * 25);
        // dayPlusSign[i].setDisplayMode(DM_HIDE);
    }

    // 注册所有Pattern对象
    for (int i = 0; i < 12; i++)
        registerPattern(&inputFields[i]);
    for (int i = 0; i < 3; i++)
        registerPattern(&dayPlusSign[i]);

    switchInput();
    switchInput();
}

void TimeCtrl::drawSpecific(U8G2 *u8g2Ptr)
{
    // 外框
    u8g2Ptr->drawRFrame(getX(), getY(), 128, 130, 5);

    // 固定文字图案
    u8g2Ptr->setFont(FONT_CHN_16);
    u8g2Ptr->drawUTF8(getX() + 10, getY() + 30, "水温保持时段：");
    u8g2Ptr->setFont(FONT_INPUT_DIGIT);
    for (int i = 0; i < 3; i++)
    {
        u8g2Ptr->drawUTF8(getX() + 1 + 27 * 1, getY() + 60 + i * 25, ":");
        u8g2Ptr->drawUTF8(getX() - 1 + 27 * 2, getY() + 60 + i * 25, "-");
        u8g2Ptr->drawUTF8(getX() + 1 + 27 * 3, getY() + 60 + i * 25, ":");
    }

    // 输入字段
    for (auto &inputFields : getInputFields())
        inputFields.draw(u8g2Ptr);

    // "+1"标识
    for (int i = 0; i < 3; i++)
        dayPlusSign[i].draw(u8g2Ptr);
}

TimeSettings TimeCtrl::getSettings()
{
    TimeSettings        ret;
    vector<InputDigit> &inputFields = getInputFields();

    for (int i = 0; i < 3; i++)
    {
        ret.startHour[i]   = inputFields[i * 4 + 0].getValue();
        ret.startMinute[i] = inputFields[i * 4 + 1].getValue();
        ret.endHour[i]     = inputFields[i * 4 + 2].getValue();
        ret.endMinute[i]   = inputFields[i * 4 + 3].getValue();
    }
    return ret;
}

void TimeCtrl::setData(TimeSettings set)
{
    vector<InputDigit> &inputFields = getInputFields();

    for (int i = 0; i < 3; i++)
    {
        inputFields[i * 4].setValue(set.startHour[i]);
        inputFields[i * 4 + 1].setValue(set.startMinute[i]);
        inputFields[i * 4 + 2].setValue(set.endHour[i]);
        inputFields[i * 4 + 3].setValue(set.endMinute[i]);
    }
}

void TimeCtrl::inputHandler()
{
    int                 beginT;
    int                 endT;
    vector<InputDigit> &inputFields = getInputFields();

    for (int i = 0; i < 3; i++)
    {
        beginT = inputFields[i * 4].getValue() * 60 + inputFields[i * 4 + 1].getValue();
        endT   = inputFields[i * 4 + 2].getValue() * 60 + inputFields[i * 4 + 3].getValue();
        if (endT < beginT)
            dayPlusSign[i].setDisplayMode(DM_SHOW);
        else
            dayPlusSign[i].setDisplayMode(DM_HIDE);

        if (beginT == 0 && endT % 60 == 0)
            inputFields[i * 4 + 2].setLimit(0, 24);
        else
            inputFields[i * 4 + 2].setLimit(0, 23);
    }
}

/*************************************************************/
/*                         其他面板                           */
/*************************************************************/

PumpIndicator::PumpIndicator()
: pumpSym((uint8_t *)cycle, 1300, 6)
, currentSym((uint8_t *)FONT_DISP_MID, PT_FONT)
{
    pumpSym.setPosition(14, 5);
    pumpSym.setBMPSize(100, 100);
    pumpSym.setRollingInterval(100);

    currentSym.setPosition(44, 58);

    setPumpOnOff(true);
    setWaterCurrent(3.5);
}

void PumpIndicator::drawSpecific(U8G2 *u8g2Ptr)
{
    pumpSym.draw(u8g2Ptr);

    char buf[5];
    if (waterCurrent > 9.9) waterCurrent = 9.9;
    snprintf(buf, sizeof(buf), "%.1f", waterCurrent);
    currentSym.setCode(buf);

    currentSym.draw(u8g2Ptr);

    u8g2Ptr->setFont(FONT_SMALL_ENG);
    u8g2Ptr->drawStr(getX() + 46, getY() + 73, "L/min");
    // u8g2Ptr->drawStr(getX() + 55, getY() + 73, "/");
}

void PumpIndicator::setPumpOnOff(bool isOn)
{
    pumpIsOn = isOn;
    pumpSym.setRollOnOff(isOn);
}

void PumpIndicator::setWaterCurrent(float current) { waterCurrent = current; }