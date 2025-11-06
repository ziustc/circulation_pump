#ifndef PATTERN_H
#define PATTERN_H

#include <stdint.h>
#include "U8g2lib.h"

enum DisplayMode
{
    DM_SHOW,
    DM_HIDE,
    DM_FLASH
};

enum PatternType
{
    PT_BMP,
    PT_FONT
};

class Pattern
{
public:
    Pattern(U8G2 *u8g2Ptr = nullptr, uint8_t *font = nullptr, PatternType type = PT_BMP);
    void         setU8G2(U8G2 *u8g2Ptr);
    void         setFont(uint8_t *font);
    void         setPosition(uint16_t x, uint16_t y);
    void         setBMPSize(uint16_t w, uint16_t h);
    void         movePosition(int dx, int dy);
    void         setCode(char *code);
    void         setDisplayMode(DisplayMode mode);
    DisplayMode  getDisplayMode();
    void         setFlashInterval(int interval);
    virtual void draw(); // 虚函数，每次刷新LCD时调用，在各派生类中预处理完成后，调用基类drawCore()实现写入u8g2

protected:
    void drawCore(); // 由draw()调用，将本Pattern（包括子类的各种类型）的图案输出到缓冲区

private:
    U8G2       *u8g2;        // 指向U8G2对象的指针
    uint8_t    *fontSet;     // 图案集（自定义的位图数组）或字库名称
    PatternType patternType; // 是图案集还是字库
    char       *patternCode; // 若fontSet是图案数组(patternTpye=PT_BMP)，*code是要显示的数组下标（code本身还是指针）
                             // 若fontSet是字库(patternType=PT_FONT)，code是字符串起始指针，以\0结尾
    int16_t        posX;
    int16_t        posY;
    int16_t        width;          // 仅针对patternType==PT_BMP
    int16_t        height;         // 仅针对patternType==PT_BMP
    DisplayMode    mode;           // 显示模式（show/hide/flash）
    bool           isVisibleNow;   // 当前是否可见
    long           lastSwitchTime; // flash状态时的上次切换时间
    int            flashInterval;  // 闪烁间隔时间
    const uint8_t *bmpPtr();       //针对fontSet是图案数组(patternTpye=PT_BMP)的，计算具体的图案指针
};

class InputDigit : public Pattern
{
public:
    InputDigit(U8G2 *u8g2Ptr = nullptr, uint8_t *font = nullptr);
    void setLimit(int max, int min);
    void setDigitCount(int digitCount); // 设置显示的数字位数
    void setValue(int number);
    int  getValue();
    void increase();
    void decrease();
    void draw() override; // 重写draw()，然后调用基类的drawCore，显示数字

private:
    int maxValue; // 最大数字
    int minValue; // 最小数字
    int digitCnt; // 显示的数字位数
    int value;    // 当前数值
};

class MultiSymbol : public Pattern
{
public:
    MultiSymbol(U8G2 *u8g2Ptr, uint8_t *font, uint8_t count);
    void setSymbolCount(uint8_t count);    // 设置符号的总数
    void setSymbolIndex(uint8_t index);    // 设置当前符号的位置编号
    void setRollingInterval(int interval); // 设置符号滚动间隔时间
    void rollSymbol();                     // 向前滚动符号
    void rollSymbolBack();                 // 向后滚动符号
    void draw() override;                  // 重写draw()，然后调用基类的drawCore，显示图案

private:
    uint8_t symbolCnt;       // 符号数量
    uint8_t symbolIndex;     // 当前显示的符号ID
    long    lastRollingTime; // 上次符号滚动时间
    int     rollingInterval; // 符号滚动间隔
};

#endif