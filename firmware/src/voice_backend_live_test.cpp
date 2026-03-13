#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>
#include <math.h>
#include <mbedtls/base64.h>

#include "voice_backend_test_config.h"

using namespace websockets;

namespace {
constexpr i2s_port_t kSpkI2sPort = I2S_NUM_0;
constexpr i2s_port_t kMicI2sPort = I2S_NUM_1;
constexpr int kSpkSampleRate = 24000;
constexpr int kMicSampleRate = 16000;
constexpr int kSpkBclkPin = 4;
constexpr int kSpkLrcPin = 5;
constexpr int kSpkDinPin = 6;
constexpr int kMicBclkPin = 15;
constexpr int kMicWsPin = 16;
constexpr int kMicDinPin = 17;
constexpr int kBitsPerSample = 16;
constexpr size_t kMicFrameSamples = 320;  // 20ms at 16kHz
constexpr size_t kMicChunkBytes = kMicFrameSamples * sizeof(int16_t);
constexpr size_t kMicBase64Cap = 1024;
constexpr size_t kTtsChunkBytes = 1024;
constexpr size_t kWsJsonDocBytes = 4096;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;
constexpr uint32_t kVadCalibrationMs = 2500;
constexpr float kNoiseEwmaAlpha = 0.08f;
constexpr float kVoiceOnFactor = 2.8f;
constexpr float kVoiceOffFactor = 1.8f;
constexpr float kVoiceOnMinRms = 0.012f;
constexpr float kVoiceOffMinRms = 0.0065f;
constexpr float kInterruptOnFactor = 3.4f;
constexpr float kInterruptMinRms = 0.015f;
constexpr uint8_t kInterruptTriggerFrames = 2;
constexpr uint32_t kVoiceOffHoldMs = 450;
constexpr uint32_t kTailStreamMs = 350;

WebsocketsClient g_client;
String g_sessionId;
bool g_wsReady = false;
bool g_voiceSessionActive = false;
bool g_ttsStreaming = false;
bool g_canCapture = true;
bool g_waitingForReply = false;
bool g_interruptArmed = false;
float g_noiseFloor = 0.0f;
bool g_noiseReady = false;
bool g_voiceActive = false;
uint32_t g_bootMs = 0;
uint32_t g_lastVoiceOnMs = 0;
uint32_t g_lastStreamMs = 0;
uint8_t g_interruptFrames = 0;

const char* wifiStatusText(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD:
      return "NO_SHIELD";
    case WL_IDLE_STATUS:
      return "IDLE";
    case WL_NO_SSID_AVAIL:
      return "NO_SSID";
    case WL_SCAN_COMPLETED:
      return "SCAN_DONE";
    case WL_CONNECTED:
      return "CONNECTED";
    case WL_CONNECT_FAILED:
      return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "DISCONNECTED";
    default:
      return "UNKNOWN";
  }
}

void initSpeakerI2s() {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = kSpkSampleRate,
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
      .bck_io_num = kSpkBclkPin,
      .ws_io_num = kSpkLrcPin,
      .data_out_num = kSpkDinPin,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };

  i2s_driver_install(kSpkI2sPort, &config, 0, nullptr);
  i2s_set_pin(kSpkI2sPort, &pins);
  i2s_zero_dma_buffer(kSpkI2sPort);
}

void initMicI2s() {
  i2s_config_t cfg = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = kMicSampleRate,
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
      .bck_io_num = kMicBclkPin,
      .ws_io_num = kMicWsPin,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = kMicDinPin,
  };

  i2s_driver_install(kMicI2sPort, &cfg, 0, nullptr);
  i2s_set_pin(kMicI2sPort, &pinCfg);
  i2s_zero_dma_buffer(kMicI2sPort);
}

void sendJson(const JsonDocument& doc) {
  String payload;
  serializeJson(doc, payload);
  g_client.send(payload);
}

void sendHello() {
  JsonDocument doc;
  doc["type"] = "hello";
  doc["device_id"] = "esp32-s3-voice-live-test";
  doc["session_id"] = g_sessionId;
  sendJson(doc);
}

void startVoiceSession() {
  g_sessionId = "voice-" + String(static_cast<uint32_t>(millis()));
  JsonDocument doc;
  doc["type"] = "start_session";
  doc["device_id"] = "esp32-s3-voice-live-test";
  doc["session_id"] = g_sessionId;
  sendJson(doc);
}

void stopVoiceSession() {
  JsonDocument doc;
  doc["type"] = "stop_session";
  doc["device_id"] = "esp32-s3-voice-live-test";
  doc["session_id"] = g_sessionId;
  sendJson(doc);
}

void sendInterrupt() {
  JsonDocument doc;
  doc["type"] = "interrupt";
  doc["device_id"] = "esp32-s3-voice-live-test";
  doc["session_id"] = g_sessionId;
  sendJson(doc);
}

bool encodeBase64(const uint8_t* input, size_t inputLen, char* output, size_t outputCap) {
  size_t olen = 0;
  int rc = mbedtls_base64_encode(
      reinterpret_cast<unsigned char*>(output),
      outputCap,
      &olen,
      input,
      inputLen);
  if (rc != 0 || olen == 0 || olen >= outputCap) {
    return false;
  }
  output[olen] = '\0';
  return true;
}

bool decodeBase64(const String& input, uint8_t* output, size_t outputCap, size_t& outputLen) {
  size_t olen = 0;
  int rc = mbedtls_base64_decode(
      output,
      outputCap,
      &olen,
      reinterpret_cast<const unsigned char*>(input.c_str()),
      input.length());
  if (rc != 0) {
    return false;
  }
  outputLen = olen;
  return true;
}

void playPcmChunk(const String& payloadB64) {
  uint8_t raw[kTtsChunkBytes] = {0};
  size_t decodedLen = 0;
  if (!decodeBase64(payloadB64, raw, sizeof(raw), decodedLen)) {
    Serial.println("Failed to decode tts_chunk");
    return;
  }
  size_t written = 0;
  i2s_write(kSpkI2sPort, raw, decodedLen, &written, portMAX_DELAY);
}

bool connectWifi() {
  Serial.print("Connecting WiFi: ");
  Serial.println(kVoiceTestWifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(kVoiceTestWifiSsid, kVoiceTestWifiPassword);

  uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < kWifiConnectTimeoutMs) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("WiFi failed: ");
    Serial.println(wifiStatusText(WiFi.status()));
    return false;
  }

  Serial.print("WiFi OK, IP=");
  Serial.println(WiFi.localIP());
  return true;
}

bool connectBackend() {
  String url = "ws://";
  url += kVoiceTestBackendHost;
  url += ":";
  url += String(kVoiceTestBackendPort);
  url += kVoiceTestBackendPath;

  Serial.print("Connecting backend: ");
  Serial.println(url);
  g_wsReady = g_client.connect(url);
  Serial.println(g_wsReady ? "Backend WS connected" : "Backend WS connect failed");
  return g_wsReady;
}

void onMessageCallback(WebsocketsMessage message) {
  DynamicJsonDocument doc(kWsJsonDocBytes);
  DeserializationError err = deserializeJson(doc, message.data());
  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return;
  }

  const char* type = doc["type"] | "";
  const char* detail = doc["detail"] | "";
  const char* text = doc["text"] | "";

  Serial.print("WS type=");
  Serial.print(type);
  if (strlen(detail) > 0) {
    Serial.print(" detail=");
    Serial.print(detail);
  }
  if (strlen(text) > 0) {
    Serial.print(" text=");
    Serial.print(text);
  }
  Serial.println();

  if (strcmp(type, "session_started") == 0) {
    g_voiceSessionActive = true;
    g_waitingForReply = false;
    g_interruptArmed = false;
    g_interruptFrames = 0;
  } else if (strcmp(type, "asr_partial") == 0 || strcmp(type, "asr_final") == 0) {
    // Logged above.
  } else if (strcmp(type, "backend_status") == 0) {
    if (strcmp(detail, "interrupt_ack") == 0 || strcmp(detail, "session_stop_ack") == 0) {
      g_ttsStreaming = false;
      g_voiceSessionActive = false;
      g_waitingForReply = false;
      g_interruptArmed = false;
      g_interruptFrames = 0;
      Serial.println("Backend session cleared -> listening");
    }
  } else if (strcmp(type, "tts_chunk") == 0) {
    if (g_interruptArmed) {
      return;
    }
    g_ttsStreaming = true;
    playPcmChunk(String(static_cast<const char*>(doc["payload_b64"] | "")));
  } else if (strcmp(type, "tts_end") == 0) {
    g_ttsStreaming = false;
    g_voiceSessionActive = false;
    g_waitingForReply = false;
    g_interruptArmed = false;
    g_interruptFrames = 0;
    Serial.println("Round complete");
  }
}

bool readMicFrame(int16_t* outPcm, size_t samples, float& rmsNorm) {
  int32_t raw[kMicFrameSamples];
  size_t bytesRead = 0;
  esp_err_t err = i2s_read(kMicI2sPort, raw, samples * sizeof(int32_t), &bytesRead, pdMS_TO_TICKS(40));
  if (err != ESP_OK || bytesRead == 0) {
    return false;
  }

  size_t count = bytesRead / sizeof(int32_t);
  if (count == 0) {
    return false;
  }

  double sumSq = 0.0;
  for (size_t i = 0; i < count; ++i) {
    int32_t s24 = raw[i] >> 8;
    outPcm[i] = static_cast<int16_t>(s24 >> 8);
    sumSq += static_cast<double>(s24) * static_cast<double>(s24);
  }

  rmsNorm = sqrt(sumSq / count) / 8388608.0f;
  return true;
}

void sendAudioChunk(const int16_t* pcm, size_t bytes) {
  char b64[kMicBase64Cap] = {0};
  if (!encodeBase64(reinterpret_cast<const uint8_t*>(pcm), bytes, b64, sizeof(b64))) {
    Serial.println("Base64 encode failed");
    return;
  }

  JsonDocument doc;
  doc["type"] = "audio_chunk";
  doc["device_id"] = "esp32-s3-voice-live-test";
  doc["session_id"] = g_sessionId;
  doc["sample_rate"] = kMicSampleRate;
  doc["payload_b64"] = b64;
  sendJson(doc);
}

void updateVoiceUplink() {
  if (!g_wsReady || !g_canCapture) {
    return;
  }

  int16_t pcm[kMicFrameSamples] = {0};
  float rmsNorm = 0.0f;
  if (!readMicFrame(pcm, kMicFrameSamples, rmsNorm)) {
    return;
  }

  uint32_t now = millis();
  if (!g_noiseReady) {
    g_noiseFloor = (g_noiseFloor <= 0.0f)
                       ? rmsNorm
                       : (1.0f - kNoiseEwmaAlpha) * g_noiseFloor + kNoiseEwmaAlpha * rmsNorm;
    if ((now - g_bootMs) >= kVadCalibrationMs) {
      g_noiseReady = true;
      Serial.println("VAD calibrated");
    }
    return;
  }

  if (!g_voiceActive && !g_voiceSessionActive) {
    g_noiseFloor = (1.0f - kNoiseEwmaAlpha) * g_noiseFloor + kNoiseEwmaAlpha * rmsNorm;
  }

  float voiceOnTh = max(kVoiceOnMinRms, g_noiseFloor * kVoiceOnFactor);
  float voiceOffTh = max(kVoiceOffMinRms, g_noiseFloor * kVoiceOffFactor);
  float interruptOnTh = max(kInterruptMinRms, g_noiseFloor * kInterruptOnFactor);
  bool aboveOn = rmsNorm >= voiceOnTh;
  bool belowOff = rmsNorm < voiceOffTh;

  if (g_ttsStreaming) {
    if (rmsNorm >= interruptOnTh) {
      if (g_interruptFrames < 255) {
        ++g_interruptFrames;
      }
      Serial.print("INT?");
      Serial.print(" rms=");
      Serial.print(rmsNorm, 4);
      Serial.print(" th=");
      Serial.print(interruptOnTh, 4);
      Serial.print(" frames=");
      Serial.println(g_interruptFrames);
    } else {
      g_interruptFrames = 0;
    }

    if (!g_interruptArmed && g_interruptFrames >= kInterruptTriggerFrames) {
      g_interruptArmed = true;
      g_ttsStreaming = false;
      g_voiceSessionActive = false;
      g_waitingForReply = false;
      g_voiceActive = false;
      i2s_zero_dma_buffer(kSpkI2sPort);
      sendInterrupt();
      Serial.println("TTS interrupted -> ready for next question");
      g_interruptFrames = 0;
    }
    return;
  }

  if (!g_voiceActive && aboveOn && !g_ttsStreaming && !g_waitingForReply) {
    g_voiceActive = true;
    g_lastVoiceOnMs = now;
    g_lastStreamMs = now;
    Serial.println("VOICE_ON -> start session");
    startVoiceSession();
    g_waitingForReply = true;
    return;
  }

  if (!g_voiceActive) {
    return;
  }

  if (aboveOn) {
    g_lastVoiceOnMs = now;
  }

  if (g_voiceSessionActive) {
    sendAudioChunk(pcm, kMicChunkBytes);
    g_lastStreamMs = now;
  }

  if (belowOff && (now - g_lastVoiceOnMs) > kVoiceOffHoldMs) {
    if ((now - g_lastStreamMs) > kTailStreamMs) {
      g_voiceActive = false;
      Serial.println("VOICE_OFF -> wait reply");
      stopVoiceSession();
    }
  }
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("VOICE BACKEND LIVE TEST");

  if (strlen(kVoiceTestWifiSsid) == 0 || strlen(kVoiceTestWifiPassword) == 0) {
    Serial.println("Please fill firmware/include/voice_backend_test_config.h first");
    while (true) {
      delay(1000);
    }
  }

  g_bootMs = millis();
  initSpeakerI2s();
  initMicI2s();
  if (!connectWifi()) {
    return;
  }

  g_client.onMessage(onMessageCallback);
  if (!connectBackend()) {
    return;
  }

  g_sessionId = "boot-" + String(static_cast<uint32_t>(millis()));
  sendHello();
  Serial.println("Speak after calibration...");
}

void loop() {
  if (g_wsReady) {
    g_client.poll();
  }
  updateVoiceUplink();
  delay(5);
}
