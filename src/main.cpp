#include <Arduino.h>
#include <U8g2lib.h>
#include <stdio.h>

U8G2_SH1108_128X160_F_4W_HW_SPI u8g2(U8G2_R0, /* cs=*/17, /* dc=*/3, /* reset=*/8); // Example instantiation

// 让 printf 输出到 Serial
extern "C" int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        Serial.write(ptr[i]);
    }
    return len;
}

int   frameCount       = 0;
long  lastMillis       = 0;
long  lastSwitchMillis = 0;
float fps              = 0;
int   screen_num       = 0;

struct Point
{
    int x;
    int y;
};

Point pnl[2] = {{0, 0}, {140, 0}};

void setup(void)
{
    Serial.begin(115200);
    Serial.println("Hello monoTFT");

    // 测试printf
    printf("Hello, Arduino printf!\n");
    printf("Number test: %d, hex: 0x%X\n", 1234, 1234);

    u8g2.begin();
    u8g2.enableUTF8Print();
    u8g2.setBusClock(20000000); // Set bus clock to 20 MHz
}

void loop(void)
{
    long curMillis = millis();

    // 计算FPS
    frameCount++;
    if (curMillis - lastMillis >= 500)
    {
        fps        = (float)frameCount * 1000 / (curMillis - lastMillis);
        lastMillis = curMillis;
        frameCount = 0;
    }
    Serial.println("FPS: " + String(fps, 5));

    // 切换屏幕
    if (curMillis - lastSwitchMillis >= 5000)
    {
        lastSwitchMillis = curMillis;
        screen_num       = (screen_num + 1) % 2;
    }

    u8g2.clearBuffer();

    // FPS
    u8g2.setFont(u8g2_font_crox1c_tf); // w=8, h=13
    u8g2.drawStr(pnl[0].x + 50, pnl[0].y + 150, ("fps=" + String(fps, 2)).c_str());

    //////////////////////////
    //       定时面板        //
    //////////////////////////

    if (screen_num == 0)
    {
        // 边框
        u8g2.drawRFrame(pnl[0].x, pnl[0].y, 128, 160, 5);

        // 标题
        u8g2.setFont(u8g2_font_wqy16_t_gb2312b);
        u8g2.setCursor(pnl[0].x + 50, pnl[0].y + 30);
        u8g2.print("定时");

        // 时间
        u8g2.setFont(u8g2_font_logisoso16_tn); // w=9, h=19

        u8g2.drawStr(pnl[0].x + 7, pnl[0].y + 70, "00:00-24:00");
        u8g2.drawStr(pnl[0].x + 7, pnl[0].y + 100, "22:00-03:00");
        u8g2.drawStr(pnl[0].x + 7, pnl[0].y + 130, "00:00-24:00");

        u8g2.setFont(u8g2_font_busdisplay11x5_tr); // w=5, h=13
        u8g2.drawStr(pnl[0].x + 112, pnl[0].y + 90, "+1");
    }

    //////////////////////////
    //       状态面板        //
    //////////////////////////

    if (screen_num == 1)
    {
        // 边框
        u8g2.drawRFrame(pnl[0].x, pnl[0].y, 128, 75, 5);

        u8g2.setFont(u8g2_font_wqy16_t_gb2312b);
        u8g2.setCursor(pnl[0].x + 10, pnl[0].y + 30);
        u8g2.print("当前水温");

        u8g2.setFont(u8g2_font_fub25_tn); // w=36, h=45
        u8g2.drawStr(pnl[0].x + 20, pnl[0].y + 70, "26.8");
        u8g2.setFont(u8g2_font_fub14_tf); // w=21, h=26
        u8g2.drawStr(pnl[0].x + 90,
                     pnl[0].y + 60,
                     "\xB0"
                     "C");

        // 边框
        u8g2.drawRFrame(pnl[0].x, pnl[0].y + 80, 128, 80, 5);

        u8g2.setFont(u8g2_font_wqy16_t_gb2312b);
        u8g2.setCursor(pnl[0].x + 10, pnl[0].y + 100);
        u8g2.print("开水  -  秒");
        u8g2.setCursor(pnl[0].x + 25, pnl[0].y + 125);
        u8g2.print("水泵启动  分");
        u8g2.setFont(u8g2_font_logisoso16_tn);
        u8g2.drawStr(pnl[0].x + 43, pnl[0].y + 100, "2");
        u8g2.drawStr(pnl[0].x + 65, pnl[0].y + 100, "6");
        u8g2.drawStr(pnl[0].x + 91, pnl[0].y + 125, "4");
    }

    u8g2.sendBuffer(); // transfer internal memory to the display
}