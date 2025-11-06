#include <vector>
#include "U8g2lib.h"
#include "panel.h"
#include "pattern.h"

using namespace std;

/*************************************************************/
/*                           Panel                           */
/*************************************************************/

Panel::Panel()
{
    posX = 0;
    posY = 0;
}

void Panel::setU8G2(U8G2 *u8g2Ptr) { u8g2 = u8g2Ptr; }

void Panel::setPosition(uint16_t left, uint16_t top)
{
    posX = left;
    posY = top;
}

/*************************************************************/
/*                         CtrlPanel                         */
/*************************************************************/

CtrlPanel::CtrlPanel()
{
    isSelected    = false;
    isActive      = false;
    selectedField = inputFields.end();
}

/**
 * @brief 设置面板选中状态，选中时面板闪烁，但还没进入面板编辑状态
 */
void CtrlPanel::setSelected(bool sel)
{
    isSelected = sel;
    if (sel)
    {
        PanelLogo.setDisplayMode(DM_FLASH);
        PanelLogo.setFlashInterval(500);
    }
}

/**
 * @brief 设置面板激活状态，激活时进入面板编辑状态
 */
void CtrlPanel::setActive(bool active)
{
    isActive = active;
    PanelLogo.setDisplayMode(DM_SHOW);

    if (inputFields.empty()) return;

    selectedField = inputFields.begin();
    selectedField->setDisplayMode(DM_FLASH);
}

bool CtrlPanel::getActive() { return isActive; }

void CtrlPanel::switchInput()
{
    if (inputFields.empty()) return;

    selectedField->setDisplayMode(DM_SHOW);

    selectedField++;
    if (selectedField == inputFields.end()) selectedField = inputFields.begin();

    selectedField->setDisplayMode(DM_FLASH);
}

void CtrlPanel::inputUp()
{
    if (inputFields.empty()) return;

    selectedField->increase();
    inputHandler();
}

void CtrlPanel::inputDown()
{
    if (inputFields.empty()) return;

    selectedField->decrease();
    inputHandler();
}

Pattern &CtrlPanel::getPanelLogo() { return PanelLogo; }

vector<InputDigit> &CtrlPanel::getInputFields() { return inputFields; }

/*************************************************************/
/*                         WaterCtrl                         */
/*************************************************************/

WaterCtrl::WaterCtrl()
:
{
    // 设置水控Logo
    Pattern &PanelLogo = getPanelLogo();
    PanelLogo.setFont(u8g2_font_wqy16_t_gb2312b);
    PanelLogo.setPatternCode("水");
    PanelLogo.setPosition(30, 40);

    // 三个输入字段：开水最短时长，最长时长，水泵运行时长
    vector<InputDigit> &inputFields = getInputFields();
    inputFields.emplace_back(InputDigit(u8g2_font_wqy16_t_gb2312b, posX + 120, posY + 40, 9, 1, 1, 1));
    inputFields.emplace_back(InputDigit(u8g2_font_wqy16_t_gb2312b, posX + 150, posY + 40, 9, 1, 1, 1));
    inputFields.emplace_back(InputDigit(u8g2_font_wqy16_t_gb2312b, posX + 230, posY + 40, 20, 1, 2, 1));
}

void WaterCtrl::draw()
{
    // panel外框
    display.drawRectangle(getX() + 2, getY() + 2, getX() + 290 - 2, getY() + 56 - 2, ST7305_COLOR_BLACK);

    // 固定文字
    u8g2_for_st73xx.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 27, "水");
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 42, "控");
    u8g2_for_st73xx.drawUTF8(getX() + 89, getY() + 40, "开水");
    u8g2_for_st73xx.drawUTF8(getX() + 142, getY() + 40, "—");
    u8g2_for_st73xx.drawUTF8(getX() + 175, getY() + 40, "秒");
    u8g2_for_st73xx.drawUTF8(getX() + 203, getY() + 40, "保持");
    u8g2_for_st73xx.drawUTF8(getX() + 264, getY() + 40, "分");

    // panel Logo显示
    getPanelLogo().draw();

    // 输入字段
    for (auto &input : getInputFields())
        input.draw();
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

void WaterCtrl::inputHandler() { }

/*************************************************************/
/*                         TempCtrl                          */
/*************************************************************/

TempCtrl::TempCtrl()
{
    // 设置温控Logo
    Pattern &PanelLogo = getPanelLogo();
    PanelLogo.setPatternSet(u8g2_font_wqy16_t_gb2312b);
    PanelLogo.setPatternCode("温");
    PanelLogo.setPosition(30, 40);
    PanelLogo.setDisplayMode(DM_SHOW);

    // 一个输入字段，水泵起始温度（该温度正负范围内启动）
    vector<InputDigit> &inputFields = getInputFields();
    inputFields.emplace_back(InputDigit(u8g2_font_wqy16_t_gb2312b, getX() + 120, getY() + 40, 9, 1, 1, 1));

    // 当前温度显示
    curTemp.setPatternSet(u8g2_font_wqy16_t_gb2312b);
    curTemp.setPatternCode("30");
    curTemp.setPosition(getX() + 100, getY() + 50);
    curTemp.setDisplayMode(DM_SHOW);
}

void TempCtrl::draw()
{
    // panel外框
    display.drawRectangle(getX() + 2, getY() + 2, getX() + 290 - 2, getY() + 56 - 2, ST7305_COLOR_BLACK);

    // 固定文字
    u8g2_for_st73xx.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 27, "温");
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 42, "控");
    u8g2_for_st73xx.drawUTF8(getX() + 89, getY() + 40, "设定水温");
    u8g2_for_st73xx.drawUTF8(getX() + 142, getY() + 40, "当前水温");
    u8g2_for_st73xx.drawUTF8(getX() + 175, getY() + 40, "°C");
    u8g2_for_st73xx.drawUTF8(getX() + 203, getY() + 40, "°C");

    // panel Logo显示
    getPanelLogo().draw();

    // 输入字段（设定温度）
    for (auto &input : getInputFields())
        input.draw();

    // 当前温度
    string strTemp = to_string(tempC);
    curTemp.setPatternCode(strTemp.c_str());
    curTemp.draw();
}

void TempCtrl::setCurTemp(int temp) { tempC = temp; }

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
    // 设置温控Logo
    Pattern &PanelLogo = getPanelLogo();
    PanelLogo.setPatternSet(u8g2_font_wqy16_t_gb2312b);
    PanelLogo.setPatternCode("时");
    PanelLogo.setPosition(30, 40);
    PanelLogo.setDisplayMode(DM_SHOW);

    // 12个输入字段，三组循环泵有效时段
    vector<InputDigit> &inputFields = getInputFields();
    for (int i = 0; i < 3; i++)
    {
        inputFields.emplace_back(InputDigit(u8g2_font_wqy16_t_gb2312b, getX() + 80, getY() + i * 20, 0, 23, 2, 0));
        inputFields.emplace_back(InputDigit(u8g2_font_wqy16_t_gb2312b, getX() + 120, getY() + i * 20, 0, 59, 2, 0));
        inputFields.emplace_back(InputDigit(u8g2_font_wqy16_t_gb2312b, getX() + 200, getY() + i * 20, 0, 23, 2, 0));
        inputFields.emplace_back(InputDigit(u8g2_font_wqy16_t_gb2312b, getX() + 240, getY() + i * 20, 0, 59, 2, 0));
    }

    // +1天提示
    for (int i = 0; i < 3; i++)
    {
        dayPlusSign.emplace_back(Pattern(u8g2_font_wqy16_t_gb2312b, "+1", getX() + 270, getY() + i * 20));
        dayPlusSign[i].setDisplayMode(DM_HIDE);
    }
}

void TimeCtrl::draw()
{
    // panel外框
    display.drawRectangle(getX() + 2, getY() + 2, getX() + 290 - 2, getY() + 56 - 2, ST7305_COLOR_BLACK);

    // panel Logo显示
    getPanelLogo().draw();

    // 输入字段
    for (auto &input : getInputFields())
        input.draw();

    // +1天提示符
    for (auto &it : dayPlusSign)
        it.draw();

    // 固定文字
    u8g2_for_st73xx.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 27, "定");
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 42, "时");
    u8g2_for_st73xx.drawUTF8(getX() + 50, getY() + 20, "1");
    u8g2_for_st73xx.drawUTF8(getX() + 50, getY() + 40, "2");
    u8g2_for_st73xx.drawUTF8(getX() + 50, getY() + 60, "3");
    u8g2_for_st73xx.drawUTF8(getX() + 170, getY() + 20, "-");
    u8g2_for_st73xx.drawUTF8(getX() + 170, getY() + 40, "-");
    u8g2_for_st73xx.drawUTF8(getX() + 170, getY() + 60, "-");
}

TimeSettings TimeCtrl::getSettings()
{
    TimeSettings        ret;
    vector<InputDigit> &inputFields = getInputFields();

    for (int i = 0; i < 3; i++)
    {
        ret.startHour[i]   = inputFields[i * 4].getValue();
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
    }
}

/*************************************************************/
/*                         其他面板                           */
/*************************************************************/

void FlowIndicator::setData(int flow) { this->flow = flow; }

void FlowIndicator::draw()
{
    display.drawRectangle(getX() + 2, getY() + 2, getX() + 98, getY() + 38, ST7305_COLOR_BLACK);
    string strFlow = to_string(flow);
    u8g2_for_st73xx.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 20, "流量");
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 20, strFlow.c_str());
}

void DurationIndicator::setData(int sec) { this->duration = sec; }

void DurationIndicator::draw()
{
    display.drawRectangle(getX() + 2, getY() + 2, getX() + 98, getY() + 38, ST7305_COLOR_BLACK);
    char timeBuf[7];
    sprintf(timeBuf, "%02d:%02ds", duration / 60, duration % 60);
    u8g2_for_st73xx.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 20, "时长");
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 20, timeBuf);
}

void CurTimeIndicator::setData(int h, int m, int s)
{
    hh = h;
    mm = m;
    ss = s;
}

void CurTimeIndicator::draw()
{
    char timeBuf[9];
    sprintf(timeBuf, "%02d:%02d:%02d", hh, mm, ss);
    u8g2_for_st73xx.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 20, timeBuf);
}

void StatusIndicator::setWifi(bool connected) { wifiConnected = connected; }

void StatusIndicator::draw()
{
    display.drawRectangle(getX() + 2, getY() + 2, getX() + 98, getY() + 38, ST7305_COLOR_BLACK);
    u8g2_for_st73xx.setFont(u8g2_font_wqy16_t_gb2312);
    u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 20, "状态");
    u8g2_for_st73xx.setFont(u8g2_font_wqy16_t_gb2312);
    if (wifiConnected)
        u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 20, "0");
    else
        u8g2_for_st73xx.drawUTF8(getX() + 10, getY() + 20, "1");
}
