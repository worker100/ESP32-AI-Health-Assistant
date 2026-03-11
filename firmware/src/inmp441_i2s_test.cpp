#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

namespace {
constexpr i2s_port_t kI2sPort = I2S_NUM_1;
constexpr int kSampleRate = 16000;
constexpr int kBclkPin = 15;
constexpr int kWsPin = 16;
constexpr int kDinPin = 17;
constexpr int kSpkBclkPin = 4;
constexpr int kSpkWsPin = 5;
constexpr int kSpkDinPin = 6;
constexpr size_t kBufferSamples = 256;
constexpr uint32_t kPrintIntervalMs = 250;
constexpr uint32_t kCalibrationMs = 3000;
constexpr float kNoiseEwmaAlpha = 0.08f;
constexpr float kVoiceOnFactor = 2.8f;
constexpr float kVoiceOffFactor = 1.8f;
constexpr float kVoiceOnMinRms = 0.012f;
constexpr float kVoiceOffMinRms = 0.0065f;
constexpr uint32_t kVoiceOffHoldMs = 450;
constexpr int32_t kClipThreshold24 = 0x7FFFF0;
}

void muteSpeakerPins() {
  // Keep MAX98357 I2S lines in a known low state during microphone test.
  pinMode(kSpkBclkPin, OUTPUT);
  pinMode(kSpkWsPin, OUTPUT);
  pinMode(kSpkDinPin, OUTPUT);
  digitalWrite(kSpkBclkPin, LOW);
  digitalWrite(kSpkWsPin, LOW);
  digitalWrite(kSpkDinPin, LOW);
}

void setupI2S() {
  i2s_config_t cfg = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = kSampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };

  i2s_pin_config_t pinCfg = {
      .bck_io_num = kBclkPin,
      .ws_io_num = kWsPin,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = kDinPin,
  };

  ESP_ERROR_CHECK(i2s_driver_install(kI2sPort, &cfg, 0, nullptr));
  ESP_ERROR_CHECK(i2s_set_pin(kI2sPort, &pinCfg));
  ESP_ERROR_CHECK(i2s_zero_dma_buffer(kI2sPort));
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("INMP441 I2S test start");
  Serial.println("Wiring: VCC->3V3 GND->GND SCK->GPIO15 WS->GPIO16 SD->GPIO17 L/R->GND");
  Serial.println("Calibrating noise floor for 3 seconds...");

  muteSpeakerPins();
  Serial.println("Speaker I2S pins forced LOW (GPIO4/5/6) to reduce hiss");

  setupI2S();
  Serial.println("I2S RX init OK");
}

void loop() {
  static uint32_t lastPrintMs = 0;
  static uint32_t bootMs = millis();
  static float noiseFloor = 0.0f;
  static bool noiseReady = false;
  static bool voiceActive = false;
  static uint32_t lastVoiceOnMs = 0;
  int32_t raw[kBufferSamples];
  size_t bytesRead = 0;

  esp_err_t err = i2s_read(kI2sPort, raw, sizeof(raw), &bytesRead, pdMS_TO_TICKS(200));
  if (err != ESP_OK || bytesRead == 0) {
    Serial.println("i2s_read timeout/error");
    return;
  }

  size_t samples = bytesRead / sizeof(int32_t);
  if (samples == 0) {
    return;
  }

  double sumSq = 0.0;
  int32_t peak24 = 0;
  int32_t meanAbs = 0;
  uint16_t clipCount = 0;

  for (size_t i = 0; i < samples; ++i) {
    // INMP441 effective data is usually 24-bit packed in 32-bit.
    int32_t s24 = raw[i] >> 8;
    int32_t a = abs(s24);
    sumSq += static_cast<double>(s24) * static_cast<double>(s24);
    meanAbs += a;
    if (a > peak24) {
      peak24 = a;
    }
    if (a >= kClipThreshold24) {
      ++clipCount;
    }
  }

  float rms24 = sqrt(sumSq / samples);
  float rmsNorm = rms24 / 8388608.0f;
  float peakNorm = peak24 / 8388608.0f;
  float meanAbsNorm = (static_cast<float>(meanAbs) / samples) / 8388608.0f;
  float clipPct = (100.0f * clipCount) / samples;

  uint32_t upMs = millis() - bootMs;
  if (!noiseReady) {
    if (noiseFloor <= 0.0f) {
      noiseFloor = rmsNorm;
    } else {
      noiseFloor = (1.0f - kNoiseEwmaAlpha) * noiseFloor + kNoiseEwmaAlpha * rmsNorm;
    }
    if (upMs >= kCalibrationMs) {
      noiseReady = true;
    }
  } else if (!voiceActive) {
    noiseFloor = (1.0f - kNoiseEwmaAlpha) * noiseFloor + kNoiseEwmaAlpha * rmsNorm;
  }

  float voiceOnTh = max(kVoiceOnMinRms, noiseFloor * kVoiceOnFactor);
  float voiceOffTh = max(kVoiceOffMinRms, noiseFloor * kVoiceOffFactor);
  if (!voiceActive && noiseReady && rmsNorm >= voiceOnTh) {
    voiceActive = true;
    lastVoiceOnMs = millis();
  } else if (voiceActive) {
    if (rmsNorm >= voiceOffTh) {
      lastVoiceOnMs = millis();
    } else if (millis() - lastVoiceOnMs > kVoiceOffHoldMs) {
      voiceActive = false;
    }
  }

  if (millis() - lastPrintMs >= kPrintIntervalMs) {
    lastPrintMs = millis();

    const char* level = "LOW";
    if (rmsNorm > 0.030f) {
      level = "LOUD";
    } else if (rmsNorm > 0.012f) {
      level = "MED";
    }

    const char* vad = voiceActive ? "VOICE_ON" : "VOICE_OFF";
    const char* cal = noiseReady ? "READY" : "CAL";

    Serial.printf(
        "RMS=%.5f Peak=%.5f MeanAbs=%.5f NF=%.5f VOn=%.5f VOff=%.5f Clip=%.1f%% Samples=%u Level=%s VAD=%s Cal=%s\n",
                  rmsNorm,
                  peakNorm,
                  meanAbsNorm,
                  noiseFloor,
                  voiceOnTh,
                  voiceOffTh,
                  clipPct,
                  static_cast<unsigned>(samples),
                  level,
                  vad,
                  cal);
  }
}
