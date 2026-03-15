#include "ui_module.h"

#include <math.h>
#include <stdio.h>

namespace ui_module {

void renderOledPage(Adafruit_SSD1306& display, const OledViewModel& vm) {
  // OLED 四宫格布局：
  // 左上 HR，右上 O2，左下 TMP，右下 FALL/GUIDE/VOICE。
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("HR");
  display.setCursor(70, 0);
  display.print("O2");

  display.setTextSize(2);
  display.setCursor(0, 10);
  if (vm.heartRateValid) {
    display.print(static_cast<int>(lroundf(vm.heartRateBpm)));
  } else {
    display.print("--");
  }

  display.setCursor(70, 10);
  if (vm.spo2Valid) {
    display.print(static_cast<int>(lroundf(vm.spo2Percent)));
    display.setTextSize(1);
    display.print("%");
    display.setTextSize(2);
  } else {
    display.print("--");
  }

  display.drawLine(0, 31, 127, 31, SSD1306_WHITE);
  display.drawLine(63, 32, 63, 63, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 34);
  display.print("TMP");
  display.setCursor(72, 34);
  display.print("FALL");

  display.setTextSize(2);
  display.setCursor(0, 44);
  if (vm.temperatureValid) {
    char tempBuf[8] = {0};
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", vm.temperatureC);
    display.print(tempBuf);
  } else {
    display.print("--.-");
  }

  // 右下区域改为双行小字号，避免 FALL 文本与下一行状态重叠。
  // 第1行：跌倒状态（fallShort）
  display.setTextSize(1);
  display.setCursor(72, 44);
  display.print(vm.fallShort);
  // 第2行：引导/语音状态（guidanceShort/voiceShort）
  display.setCursor(72, 54);
  display.print(vm.guidanceShort);
  display.print("/");
  display.print(vm.voiceShort);
  display.display();
}

}  // namespace ui_module
