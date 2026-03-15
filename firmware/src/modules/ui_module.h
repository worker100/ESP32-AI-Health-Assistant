#pragma once

#include <Adafruit_SSD1306.h>

namespace ui_module {

struct OledViewModel {
  // 上半区：HR / O2
  bool heartRateValid = false;
  float heartRateBpm = 0.0f;
  bool spo2Valid = false;
  float spo2Percent = 0.0f;

  // 下半区：TMP / FALL 状态
  bool temperatureValid = false;
  float temperatureC = 0.0f;
  const char* fallShort = "UNK";

  // 右下第二行：引导与语音状态
  const char* guidanceShort = "UNK";
  const char* voiceShort = "UNK";
};

void renderOledPage(Adafruit_SSD1306& display, const OledViewModel& vm);

}  // namespace ui_module
