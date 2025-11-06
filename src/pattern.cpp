#include "Arduino.h"
#include "u8g2lib.h"
#include "pattern.h"

////////////////////////////////////////////////////////////
//                       Pattern                          //
////////////////////////////////////////////////////////////

Pattern::Pattern(U8G2 *u8g2Ptr = nullptr, uint8_t *font = nullptr, PatternType type = PT_BMP)
{
    u8g2           = u8g2Ptr;
    fontSet        = font;
    patternType    = type;
    patternCode    = nullptr;
    posX           = 0;
    posY           = 0;
    width          = 0;
    height         = 0;
    mode           = DM_SHOW;
    isVisibleNow   = true;
    lastSwitchTime = 0;
    flashInterval  = 500; // 默认闪烁间隔500毫秒
}

void Pattern::setU8G2(U8G2 *u8g2Ptr) { u8g2 = u8g2Ptr; }

void Pattern::setFont(uint8_t *font) { fontSet = font; }

void Pattern::setPosition(uint16_t x, uint16_t y)
{
    posX = x;
    posY = y;
}

void Pattern::setBMPSize(uint16_t w, uint16_t h)
{
    width  = w;
    height = h;
}

void Pattern::movePosition(int dx, int dy)
{
    posX += dx;
    posY += dy;
}

void Pattern::setCode(char *code) { patternCode = code; }

void Pattern::setDisplayMode(DisplayMode mode)
{
    this->mode     = mode;
    lastSwitchTime = millis(); // 重置闪烁时间
    if (mode == DM_SHOW)
    {
        isVisibleNow = true;
    }
    else if (mode == DM_HIDE)
    {
        isVisibleNow = false;
    }
    // 如果是闪烁模式，保持当前状态
}

DisplayMode Pattern::getDisplayMode() { return mode; }

void Pattern::setFlashInterval(int interval) { flashInterval = interval; }

void Pattern::draw()
{
    long currentTime = millis();
    if (mode == DM_HIDE)
    {
        isVisibleNow = false;
    }

    else if (mode == DM_SHOW)
    {
        isVisibleNow = true;
    }

    else // if (mode == DM_FLASH)
    {
        // 如果是闪烁模式，检查是否需要切换可见状态
        if (currentTime - lastSwitchTime >= flashInterval)
        {
            isVisibleNow   = !isVisibleNow; // 切换可见状态
            lastSwitchTime = currentTime;
            Serial.println("flash");
        }
    }

    if (u8g2 && isVisibleNow)
    {
        drawSpecific(); //这里output()是虚函数，将调用派生类对象的output()函数
    }
}

void Pattern::drawSpecific()
{
    if (patternType == PT_BMP)
    {
        u8g2->drawXBMP(posX, posY, width, height, bmpPtr());
    }
    else // patternType == PT_FONT
    {
        u8g2->setFont(fontSet);
        u8g2->drawUTF8(posX, posY, patternCode);
    }
}

const uint8_t *Pattern::bmpPtr() { return fontSet + (*patternCode) * width * height / 8; }

////////////////////////////////////////////////////////////
//                     InputDigit                         //
////////////////////////////////////////////////////////////

InputDigit::InputDigit(U8G2 *u8g2Ptr, uint8_t *font)
: Pattern(u8g2Ptr, font, PT_FONT)
{
    maxValue = 9;
    minValue = 0;
    value    = 0;
    digitCnt = 1;
}

void InputDigit::setLimit(int max, int min)
{
    maxValue = max;
    minValue = min;
    if (value > maxValue) value = maxValue;
    if (value < minValue) value = minValue;
}

void InputDigit::setDigitCount(int digitCount)
{
    if (digitCount > 0)
        digitCnt = digitCount;
    else
        digitCnt = 1;
}

void InputDigit::setValue(int number)
{
    if (number > maxValue)
        value = maxValue;
    else if (number < minValue)
        value = minValue;
    else
        value = number;
}

int InputDigit::getValue() { return value; }

void InputDigit::increase()
{
    if (value < maxValue)
        value++;
    else
        value = minValue; // 循环到最小值
}

void InputDigit::decrease()
{
    if (value > minValue)
        value--;
    else
        value = maxValue; // 循环到最大值
}

void InputDigit::drawSpecific()
{
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%0*d", digitCnt, value);
    setCode(buffer);
    u8g2->drawUTF8(posX, posY, patternCode);
}

////////////////////////////////////////////////////////////
//                     MultiSymbol                        //
////////////////////////////////////////////////////////////

MultiSymbol::MultiSymbol(U8G2 *u8g2Ptr, uint8_t *symbolList, uint8_t count)
: Pattern(u8g2Ptr, symbolList, PT_BMP)
{
    symbolCnt       = count;
    symbolIndex     = 0;
    lastRollingTime = 0;
    rollingInterval = 100; // 默认滚动间隔100毫秒
}

void MultiSymbol::setSymbolCount(uint8_t count)
{
    symbolCnt = count;
    if (symbolIndex >= symbolCnt) symbolIndex = symbolCnt - 1;
}

void MultiSymbol::setSymbolIndex(uint8_t index)
{
    if (index >= symbolIndex)
        symbolIndex = symbolCnt - 1;
    else
        symbolIndex = index;
}

void MultiSymbol::setRollingInterval(int interval) { rollingInterval = interval; }

void MultiSymbol::rollSymbol() { symbolIndex = (symbolIndex + 1) % symbolCnt; }

void MultiSymbol::rollSymbolBack() { symbolIndex = (symbolIndex - 1 + symbolCnt) % symbolCnt; }

void MultiSymbol::drawSpecific()
{
    long currentTime = millis();
    if (currentTime - lastRollingTime >= rollingInterval)
    {
        rollSymbol();
        lastRollingTime = currentTime;
    }
    setCode((char *)(&symbolIndex));
    u8g2->drawXBMP(posX, posY, width, height, bmpPtr());
}
