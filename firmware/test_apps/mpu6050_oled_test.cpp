#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>
#include <math.h>

namespace {
constexpr uint8_t kI2cSda = 8;
constexpr uint8_t kI2cScl = 9;
constexpr uint8_t kOledAddr = 0x3C;
constexpr uint8_t kMpuAddr0 = 0x68;
constexpr uint8_t kMpuAddr1 = 0x69;
constexpr uint8_t kWhoAmIReg = 0x75;
constexpr uint8_t kPwrMgmt1Reg = 0x6B;
constexpr uint8_t kAccelConfigReg = 0x1C;
constexpr uint8_t kGyroConfigReg = 0x1B;
constexpr uint8_t kAccelStartReg = 0x3B;

constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr uint32_t kRefreshMs = 150;
constexpr uint32_t kAlarmContinuousMs = 12000;
constexpr uint32_t kAlarmMonitorMs = 30000;
constexpr uint32_t kImpactWindowMs = 900;
constexpr uint32_t kStillWindowMs = 1200;
constexpr uint32_t kCooldownMs = 3000;
constexpr float kFreeFallThresholdG = 0.72f;
constexpr float kImpactThresholdG = 1.15f;
constexpr float kImpactRotationThresholdDps = 35.0f;
constexpr float kStillAccelMinG = 0.85f;
constexpr float kStillAccelMaxG = 1.15f;
constexpr float kStillGyroThresholdDps = 32.0f;
constexpr float kRecoveryAccelThresholdG = 1.30f;
constexpr float kRecoveryGyroThresholdDps = 60.0f;

constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr int kSampleRate = 16000;
constexpr int kBclkPin = 4;
constexpr int kLrcPin = 5;
constexpr int kDinPin = 6;
constexpr int kBitsPerSample = 16;
constexpr size_t kAudioBufferSamples = 256;

Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);

enum class FallState {
  Normal,
  FreeFall,
  Impact,
  Alert,
  Monitor,
};

struct MpuSample {
  float axG = 0.0f;
  float ayG = 0.0f;
  float azG = 0.0f;
  float gxDps = 0.0f;
  float gyDps = 0.0f;
  float gzDps = 0.0f;
  float accelMagG = 0.0f;
  float gyroMaxDps = 0.0f;
  bool valid = false;
};

bool g_oledReady = false;
bool g_mpuReady = false;
bool g_i2sReady = false;
uint8_t g_mpuAddr = 0;
uint32_t g_lastRefreshMs = 0;
uint32_t g_freeFallMs = 0;
uint32_t g_impactMs = 0;
uint32_t g_alertStartMs = 0;
uint32_t g_lastStillMs = 0;
uint32_t g_cooldownUntilMs = 0;
uint32_t g_nextPulseMs = 0;
FallState g_fallState = FallState::Normal;
MpuSample g_sample;

bool probeAddress(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool writeReg8(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const size_t got = Wire.requestFrom(static_cast<int>(addr), static_cast<int>(len),
                                      static_cast<int>(true));
  if (got != len) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buf[i] = Wire.read();
  }
  return true;
}

bool readReg8(uint8_t addr, uint8_t reg, uint8_t* valueOut) {
  return readBytes(addr, reg, valueOut, 1);
}

int16_t readInt16(const uint8_t* buf, size_t offset) {
  return static_cast<int16_t>((static_cast<uint16_t>(buf[offset]) << 8) | buf[offset + 1]);
}

const char* fallStateText(FallState state) {
  switch (state) {
    case FallState::Normal:
      return "NORMAL";
    case FallState::FreeFall:
      return "FREEFALL";
    case FallState::Impact:
      return "IMPACT";
    case FallState::Alert:
      return "ALERT";
    case FallState::Monitor:
      return "MONITOR";
  }
  return "UNKNOWN";
}

void drawLines(const char* l1, const char* l2, const char* l3, const char* l4, const char* l5) {
  if (!g_oledReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(l1);
  display.println(l2);
  display.println(l3);
  display.println(l4);
  display.println(l5);
  display.display();
}

void initDisplay() {
  g_oledReady = display.begin(SSD1306_SWITCHCAPVCC, kOledAddr);
  if (g_oledReady) {
    drawLines("FALL TEST", "OLED OK", "MPU6050 + MAX98357", "GPIO8/9 + I2S", "Booting...");
  } else {
    Serial.println("OLED init failed at 0x3C");
  }
}

void printScan() {
  Serial.println("I2C scan start...");
  for (uint8_t addr = 1; addr < 127; ++addr) {
    if (probeAddress(addr)) {
      Serial.print("  found 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
    }
  }
  Serial.println("I2C scan done.");
}

bool initMpuAt(uint8_t addr) {
  uint8_t who = 0;
  if (!probeAddress(addr) || !readReg8(addr, kWhoAmIReg, &who)) {
    return false;
  }

  Serial.print("MPU probe 0x");
  if (addr < 16) {
    Serial.print('0');
  }
  Serial.print(addr, HEX);
  Serial.print(" WHO_AM_I=0x");
  if (who < 16) {
    Serial.print('0');
  }
  Serial.println(who, HEX);

  if (who != 0x68 && who != 0x69) {
    return false;
  }

  if (!writeReg8(addr, kPwrMgmt1Reg, 0x00)) {
    return false;
  }
  delay(50);

  if (!writeReg8(addr, kAccelConfigReg, 0x00)) {
    return false;
  }
  if (!writeReg8(addr, kGyroConfigReg, 0x00)) {
    return false;
  }

  g_mpuReady = true;
  g_mpuAddr = addr;
  return true;
}

void initMpu() {
  g_mpuReady = false;
  g_mpuAddr = 0;

  if (initMpuAt(kMpuAddr0)) {
    Serial.println("MPU6050 initialized at 0x68");
    return;
  }

  if (initMpuAt(kMpuAddr1)) {
    Serial.println("MPU6050 initialized at 0x69");
    return;
  }

  Serial.println("MPU6050 init failed on 0x68/0x69");
}

void initI2s() {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = kSampleRate,
      .bits_per_sample = static_cast<i2s_bits_per_sample_t>(kBitsPerSample),
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
  };

  const i2s_pin_config_t pins = {
      .bck_io_num = kBclkPin,
      .ws_io_num = kLrcPin,
      .data_out_num = kDinPin,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };

  if (i2s_driver_install(kI2sPort, &config, 0, nullptr) == ESP_OK &&
      i2s_set_pin(kI2sPort, &pins) == ESP_OK) {
    i2s_zero_dma_buffer(kI2sPort);
    g_i2sReady = true;
    Serial.println("MAX98357 I2S init OK");
  } else {
    g_i2sReady = false;
    Serial.println("MAX98357 I2S init failed");
  }
}

void playTone(float frequencyHz, uint16_t durationMs, float amplitude = 0.35f) {
  if (!g_i2sReady) {
    return;
  }

  int16_t samples[kAudioBufferSamples] = {0};
  const float omega = 2.0f * PI * frequencyHz / static_cast<float>(kSampleRate);
  const uint32_t totalSamples = static_cast<uint32_t>(kSampleRate) * durationMs / 1000U;
  uint32_t produced = 0;

  while (produced < totalSamples) {
    const size_t chunk = min<size_t>(kAudioBufferSamples, totalSamples - produced);
    for (size_t i = 0; i < chunk; ++i) {
      const float phase = omega * static_cast<float>(produced + i);
      samples[i] = static_cast<int16_t>(sinf(phase) * amplitude * 32767.0f);
    }

    size_t written = 0;
    i2s_write(kI2sPort, samples, chunk * sizeof(int16_t), &written, portMAX_DELAY);
    produced += chunk;
  }
}

void playSilence(uint16_t durationMs) {
  if (!g_i2sReady) {
    delay(durationMs);
    return;
  }

  int16_t samples[kAudioBufferSamples] = {0};
  const uint32_t totalSamples = static_cast<uint32_t>(kSampleRate) * durationMs / 1000U;
  uint32_t produced = 0;

  while (produced < totalSamples) {
    const size_t chunk = min<size_t>(kAudioBufferSamples, totalSamples - produced);
    size_t written = 0;
    i2s_write(kI2sPort, samples, chunk * sizeof(int16_t), &written, portMAX_DELAY);
    produced += chunk;
  }
}

void playAlarmBurst() {
  playTone(880.0f, 160);
  playSilence(70);
  playTone(1320.0f, 180);
  playSilence(120);
}

void readMpuSample() {
  g_sample.valid = false;
  if (!g_mpuReady) {
    return;
  }

  uint8_t buf[14] = {0};
  if (!readBytes(g_mpuAddr, kAccelStartReg, buf, sizeof(buf))) {
    return;
  }

  const int16_t axRaw = readInt16(buf, 0);
  const int16_t ayRaw = readInt16(buf, 2);
  const int16_t azRaw = readInt16(buf, 4);
  const int16_t gxRaw = readInt16(buf, 8);
  const int16_t gyRaw = readInt16(buf, 10);
  const int16_t gzRaw = readInt16(buf, 12);

  g_sample.axG = static_cast<float>(axRaw) / 16384.0f;
  g_sample.ayG = static_cast<float>(ayRaw) / 16384.0f;
  g_sample.azG = static_cast<float>(azRaw) / 16384.0f;
  g_sample.gxDps = static_cast<float>(gxRaw) / 131.0f;
  g_sample.gyDps = static_cast<float>(gyRaw) / 131.0f;
  g_sample.gzDps = static_cast<float>(gzRaw) / 131.0f;
  g_sample.accelMagG = sqrtf(g_sample.axG * g_sample.axG + g_sample.ayG * g_sample.ayG +
                             g_sample.azG * g_sample.azG);
  g_sample.gyroMaxDps =
      max(max(fabsf(g_sample.gxDps), fabsf(g_sample.gyDps)), fabsf(g_sample.gzDps));
  g_sample.valid = true;
}

void updateFallState(uint32_t now) {
  if (!g_sample.valid) {
    return;
  }

  const bool stillNow = g_sample.accelMagG >= kStillAccelMinG &&
                        g_sample.accelMagG <= kStillAccelMaxG &&
                        g_sample.gyroMaxDps <= kStillGyroThresholdDps;
  if (stillNow) {
    g_lastStillMs = now;
  }

  const bool recoveredNow =
      g_sample.accelMagG >= kRecoveryAccelThresholdG || g_sample.gyroMaxDps >= kRecoveryGyroThresholdDps;

  switch (g_fallState) {
    case FallState::Normal:
      if (now >= g_cooldownUntilMs && g_sample.accelMagG < kFreeFallThresholdG) {
        g_fallState = FallState::FreeFall;
        g_freeFallMs = now;
      }
      break;

    case FallState::FreeFall:
      if (now - g_freeFallMs > kImpactWindowMs) {
        g_fallState = FallState::Normal;
      } else if (g_sample.accelMagG > kImpactThresholdG &&
                 g_sample.gyroMaxDps > kImpactRotationThresholdDps) {
        g_fallState = FallState::Impact;
        g_impactMs = now;
      } else if (g_sample.accelMagG > 0.90f) {
        g_fallState = FallState::Normal;
      }
      break;

    case FallState::Impact:
      if (recoveredNow) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kCooldownMs;
      } else if (now - g_impactMs > kStillWindowMs && stillNow) {
        g_fallState = FallState::Alert;
        g_alertStartMs = now;
        g_nextPulseMs = now;
      } else if (now - g_impactMs > 3000) {
        g_fallState = FallState::Normal;
      }
      break;

    case FallState::Alert:
      if (recoveredNow) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kCooldownMs;
      } else if (now - g_alertStartMs >= kAlarmContinuousMs) {
        g_fallState = FallState::Monitor;
        g_nextPulseMs = now;
      }
      break;

    case FallState::Monitor:
      if (recoveredNow) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kCooldownMs;
      } else if (now - g_alertStartMs >= (kAlarmContinuousMs + kAlarmMonitorMs)) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kCooldownMs;
      }
      break;
  }
}

void updateAlarm(uint32_t now) {
  if (g_fallState == FallState::Alert) {
    playAlarmBurst();
    return;
  }

  if (g_fallState == FallState::Monitor && now >= g_nextPulseMs) {
    playAlarmBurst();
    g_nextPulseMs = now + 3000;
  }
}

void renderSample() {
  char l1[24];
  char l2[24];
  char l3[24];
  char l4[24];
  char l5[24];

  snprintf(l1, sizeof(l1), "FALL TEST");
  snprintf(l2, sizeof(l2), "MPU:%s SPK:%s", g_mpuReady ? "OK" : "FAIL", g_i2sReady ? "OK" : "FAIL");

  if (!g_mpuReady) {
    snprintf(l3, sizeof(l3), "Addr 0x68/0x69");
    snprintf(l4, sizeof(l4), "No response");
    snprintf(l5, sizeof(l5), "See Serial");
  } else if (!g_sample.valid) {
    snprintf(l3, sizeof(l3), "Addr:0x%02X", g_mpuAddr);
    snprintf(l4, sizeof(l4), "Read failed");
    snprintf(l5, sizeof(l5), "See Serial");
  } else {
    snprintf(l3, sizeof(l3), "State:%s", fallStateText(g_fallState));
    snprintf(l4, sizeof(l4), "M:%.2fg G:%.0f", g_sample.accelMagG, g_sample.gyroMaxDps);
    snprintf(l5, sizeof(l5), "Az:%.2f Ax:%.2f", g_sample.azG, g_sample.axG);
  }

  drawLines(l1, l2, l3, l4, l5);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== FALL TEST: MPU6050 + MAX98357 ===");
  Serial.println("Wiring:");
  Serial.println("  OLED SDA -> GPIO8");
  Serial.println("  OLED SCL -> GPIO9");
  Serial.println("  MPU SDA  -> GPIO8");
  Serial.println("  MPU SCL  -> GPIO9");
  Serial.println("  MPU VCC  -> 3.3V");
  Serial.println("  GPIO4 -> MAX98357 BCLK");
  Serial.println("  GPIO5 -> MAX98357 LRC");
  Serial.println("  GPIO6 -> MAX98357 DIN");
  Serial.println("  5V    -> MAX98357 VIN");
  Serial.println("  GND   -> Common GND");

  Wire.begin(kI2cSda, kI2cScl);
  Wire.setClock(100000);
  Wire.setTimeOut(80);

  initDisplay();
  printScan();
  initMpu();
  initI2s();
  renderSample();
}

void loop() {
  const uint32_t now = millis();
  if (now - g_lastRefreshMs < kRefreshMs) {
    return;
  }
  g_lastRefreshMs = now;

  readMpuSample();
  updateFallState(now);
  renderSample();
  updateAlarm(now);

  if (!g_mpuReady) {
    Serial.println("State=NO_MPU");
    return;
  }

  if (!g_sample.valid) {
    Serial.println("State=READ_FAIL");
    return;
  }

  Serial.print("State=");
  Serial.print(fallStateText(g_fallState));
  Serial.print(" | M=");
  Serial.print(g_sample.accelMagG, 2);
  Serial.print("g | Gmax=");
  Serial.print(g_sample.gyroMaxDps, 0);
  Serial.print(" dps | A=");
  Serial.print(g_sample.axG, 2);
  Serial.print(",");
  Serial.print(g_sample.ayG, 2);
  Serial.print(",");
  Serial.println(g_sample.azG, 2);
}
