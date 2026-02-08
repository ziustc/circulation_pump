#include "Arduino.h"
#include "u8g2lib.h"
#include "pattern.h"

////////////////////////////////////////////////////////////
//                       Pattern                          //
////////////////////////////////////////////////////////////

Pattern::Pattern(uint8_t *font, PatternType type)
{
    fontSet        = font;
    patternType    = type;
    patternCode    = nullptr;
    posX           = 0;
    posY           = 0;
    width          = 0;
    height         = 0;
    dispMode       = DM_SHOW;
    isVisibleNow   = true;
    lastSwitchTime = 0;
    flashInterval  = 500; // 默认闪烁间隔500毫秒
}

void Pattern::setU8G2(U8G2 *u8g2Ptr) { u8g2 = u8g2Ptr; }

void Pattern::setFont(uint8_t *font) { fontSet = font; }

void Pattern::setPosition(U8G2 *u8g2Ptr, uint16_t x, uint16_t y)
{
    u8g2 = u8g2Ptr;
    posX = x;
    posY = y;
}

void Pattern::movePosition(int dx, int dy)
{
    posX += dx;
    posY += dy;
}

void Pattern::setBMPSize(uint16_t w, uint16_t h)
{
    width  = w;
    height = h;
}

void Pattern::setCode(const char *code) { patternCode = code; }

void Pattern::setDisplayMode(DisplayMode mode)
{
    this->dispMode = mode;
    lastSwitchTime = millis(); // 重置闪烁时间
    if (dispMode == DM_SHOW)
    {
        isVisibleNow = true;
    }
    else if (dispMode == DM_HIDE)
    {
        isVisibleNow = false;
    }
    // 如果是闪烁模式，保持当前状态
}

DisplayMode Pattern::getDisplayMode() { return dispMode; }

void Pattern::setFlashInterval(int interval) { flashInterval = interval; }

void Pattern::draw() { drawCore(); }

void Pattern::draw(U8G2 *u8g2Ptr, uint16_t x, uint16_t y)
{
    u8g2 = u8g2Ptr;
    posX = x;
    posY = y;
    draw();
    // 由于draw()是虚函数，这里会在设置好位置后，重新调用派生类的draw()函数，
    // 然后在从派生类的draw()调用基类的drawCore()完整顺序是：
    //
    // InputDigit::(继承的) Pattern::draw(u8g2, x, y)
    // ↓
    // Pattern::draw(u8g2, x, y) 的内部逻辑（设置位置后调用虚函数 draw()）
    // ↓
    // InputDigit::draw() ← 因为 draw() 是虚函数，被重写了
    // ↓
    // InputDigit::draw() 内部的绘制逻辑（例如计算数值等）
    // ↓
    // Pattern::drawCore() ← 最终执行图像写入
}

void Pattern::drawCore()
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

    if (isVisibleNow)
    {
        if (patternType == PT_BMP)
        {
            u8g2->drawXBMP(posX, posY, width, height, fontSet + *((int *)patternCode));
            // patternCode原本为char*类型，如果为图案，在派生类的draw()中会让他指向一个int（4字节）类型变量offset的地址
            // 这里(int*)patternCode先转化为int类型指针，然后读取该地址的值，得到offset，在加上fontSet基地址，得到图案首地址
        }
        else // patternType == PT_FONT
        {
            u8g2->setFont(fontSet);
            u8g2->drawUTF8(posX, posY, patternCode);
        }
    }
}

////////////////////////////////////////////////////////////
//                     InputDigit                         //
////////////////////////////////////////////////////////////

InputDigit::InputDigit(uint8_t *font)
: Pattern(font, PT_FONT)
{
    maxValue = 9;
    minValue = 0;
    value    = 0;
    digitCnt = 1;
}

void InputDigit::setLimit(int min, int max)
{
    minValue = min;
    maxValue = max;
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

void InputDigit::draw()
{
    char buffer[10];
    snprintf(buffer, sizeof(buffer), "%0*d", digitCnt, value);
    setCode(buffer);
    drawCore();
}

////////////////////////////////////////////////////////////
//                     MultiSymbol                        //
////////////////////////////////////////////////////////////

MultiSymbol::MultiSymbol(uint8_t *symbolListPtr, int singleSymbolLen, int count)
: Pattern(symbolListPtr, PT_BMP)
{
    symbolCnt       = count;
    symbolLen       = singleSymbolLen;
    symbolIndex     = 0;
    symbolRolling   = false;
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

void MultiSymbol::setRollOnOff(bool isRolling)
{
    symbolRolling = isRolling;
    if (!isRolling) lastRollingTime = 0;
}

void MultiSymbol::rollSymbol() { symbolIndex = (symbolIndex + 1) % symbolCnt; }

void MultiSymbol::rollSymbolBack() { symbolIndex = (symbolIndex - 1 + symbolCnt) % symbolCnt; }

void MultiSymbol::draw()
{
    long currentTime  = millis();
    int  symbolOffset = symbolIndex * symbolLen;

    if (symbolRolling && (currentTime - lastRollingTime >= rollingInterval))
    {
        rollSymbol();
        lastRollingTime = currentTime;
    }
    setCode((char *)(&symbolOffset));
    drawCore();
}
