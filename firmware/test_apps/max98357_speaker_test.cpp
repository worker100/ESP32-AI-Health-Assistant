#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

namespace {
constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr int kSampleRate = 16000;
constexpr int kBclkPin = 4;
constexpr int kLrcPin = 5;
constexpr int kDinPin = 6;  // ESP32 data out -> MAX98357 DIN
constexpr int kBitsPerSample = 16;
constexpr size_t kBufferSamples = 256;
constexpr float kWhisperAmplitude = 0.03f;

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

  i2s_driver_install(kI2sPort, &config, 0, nullptr);
  i2s_set_pin(kI2sPort, &pins);
  i2s_zero_dma_buffer(kI2sPort);
}

void playTone(float frequencyHz, uint16_t durationMs, float amplitude = kWhisperAmplitude) {
  int16_t samples[kBufferSamples] = {0};
  const float omega = 2.0f * PI * frequencyHz / static_cast<float>(kSampleRate);
  const uint32_t totalSamples = static_cast<uint32_t>(kSampleRate) * durationMs / 1000U;
  uint32_t produced = 0;

  while (produced < totalSamples) {
    const size_t chunk = min<size_t>(kBufferSamples, totalSamples - produced);
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
  int16_t samples[kBufferSamples] = {0};
  const uint32_t totalSamples = static_cast<uint32_t>(kSampleRate) * durationMs / 1000U;
  uint32_t produced = 0;

  while (produced < totalSamples) {
    const size_t chunk = min<size_t>(kBufferSamples, totalSamples - produced);
    size_t written = 0;
    i2s_write(kI2sPort, samples, chunk * sizeof(int16_t), &written, portMAX_DELAY);
    produced += chunk;
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("=== MAX98357 + SPEAKER TEST ===");
  Serial.println("Wiring:");
  Serial.println("  GPIO4 -> BCLK");
  Serial.println("  GPIO5 -> LRC/LRCLK");
  Serial.println("  GPIO6 -> DIN");
  Serial.println("  5V    -> VIN");
  Serial.println("  GND   -> GND");
  Serial.println("  Speaker -> SPK+ / SPK-");

  initI2s();
  Serial.print("I2S init done. Whisper amplitude=");
  Serial.println(kWhisperAmplitude, 3);
  Serial.println("Playing quiet beep loop...");
}

void loop() {
  Serial.println("Whisper beep 880Hz");
  playTone(880.0f, 140);
  playSilence(220);

  Serial.println("Whisper beep 1320Hz");
  playTone(1320.0f, 140);
  playSilence(800);
}
