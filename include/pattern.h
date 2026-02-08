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
    Pattern(uint8_t *font, PatternType type);
    void         setU8G2(U8G2 *u8g2Ptr);
    void         setFont(uint8_t *font);
    void         setPosition(U8G2 *u8g2, uint16_t x, uint16_t y);
    void         setBMPSize(uint16_t w, uint16_t h);
    void         movePosition(int dx, int dy);
    void         setCode(const char *code);
    void         setDisplayMode(DisplayMode mode);
    DisplayMode  getDisplayMode();
    void         setFlashInterval(int interval);
    virtual void draw(); // 虚函数，每次刷新LCD时调用，
                         // 在各派生类中预处理完成后，调用基类drawCore()实现写入u8g2

    virtual void draw(U8G2 *u8g2Ptr, uint16_t x, uint16_t y); // 虚函数，每次刷新LCD时调用，draw()的时候确定位置

protected:
    void drawCore(); // 由draw()调用，将本Pattern（包括子类的各种类型）的图案输出到缓冲区

private:
    U8G2       *u8g2;        // U8G2指针
    uint8_t    *fontSet;     // 图案集（自定义的位图数组）或字库名称
    PatternType patternType; // 是图案集还是字库
    const char *patternCode; // 若fontSet是图案数组(patternTpye=PT_BMP)，
                             // *code是要显示的图案的offset（使用4字节组成int类型）
                             // 若fontSet是字库(patternType=PT_FONT)，
                             // code是字符串起始指针，以\0结尾
    int16_t     posX;
    int16_t     posY;
    int16_t     width;          // 仅针对patternType==PT_BMP
    int16_t     height;         // 仅针对patternType==PT_BMP
    DisplayMode dispMode;       // 显示模式（show/hide/flash）
    bool        isVisibleNow;   // 当前是否可见
    long        lastSwitchTime; // flash状态时的上次切换时间
    int         flashInterval;  // 闪烁间隔时间
};

class InputDigit : public Pattern
{
public:
    InputDigit(uint8_t *font);
    void setLimit(int min, int max);
    void setDigitCount(int digitCount); // 设置显示的数字位数
    void setValue(int number);
    int  getValue();
    void increase();
    void decrease();
    void draw() override; // 重写draw()，然后调用基类的drawCore，显示数字
    using Pattern::draw;  // C++中，基类若有多个同名函数（参数表不同），若派生类override了其中一个，其他同名函数
                          // 也都被隐藏了。所以按语法，需要在派生类中使用using，重新把基类的这些同名函数拉出来

private:
    int minValue; // 最小数字
    int maxValue; // 最大数字
    int digitCnt; // 显示的数字位数
    int value;    // 当前数值
};

class MultiSymbol : public Pattern
{
public:
    MultiSymbol(uint8_t *symbolListPtr, int singleSymbolLen, int count);
    void setSymbolCount(uint8_t count);    // 设置符号的总数
    void setSymbolIndex(uint8_t index);    // 设置当前符号的位置编号
    void setRollingInterval(int interval); // 设置符号滚动间隔时间
    void setRollOnOff(bool isRolling);     // 启动或停止自动切换符号
    void rollSymbol();                     // 向前滚动符号
    void rollSymbolBack();                 // 向后滚动符号
    void draw() override;                  // 重写draw()，然后调用基类的drawCore，显示图案
    using Pattern::draw; // C++中，基类若有多个同名函数（参数表不同），若派生类override了其中一个，其他同名函数
                         // 也都被隐藏了。所以按语法，需要在派生类中使用using，重新把基类的这些同名函数拉出来

private:
    int  symbolCnt;       // 图案数量，即二维数组的第一维长度
    int  symbolLen;       // 图案数组中单个图案的长度，即二维数组的第二维长度
    int  symbolIndex;     // 当前显示的符号
    bool symbolRolling;   // 当前在自动切换
    long lastRollingTime; // 上次符号滚动时间
    int  rollingInterval; // 符号滚动间隔
};

#endif