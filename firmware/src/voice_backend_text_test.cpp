#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <driver/i2s.h>
#include <mbedtls/base64.h>

#include "voice_backend_test_config.h"

using namespace websockets;

namespace {
constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr int kSampleRate = 24000;
constexpr int kBclkPin = 4;
constexpr int kLrcPin = 5;
constexpr int kDinPin = 6;
constexpr int kBitsPerSample = 16;
constexpr size_t kAudioChunkBytes = 2048;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;

WebsocketsClient g_client;
String g_sessionId;
bool g_wsReady = false;
bool g_ttsStreaming = false;

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
  uint8_t raw[kAudioChunkBytes] = {0};
  size_t decodedLen = 0;
  if (!decodeBase64(payloadB64, raw, sizeof(raw), decodedLen)) {
    Serial.println("Failed to decode tts_chunk base64");
    return;
  }
  size_t written = 0;
  i2s_write(kI2sPort, raw, decodedLen, &written, portMAX_DELAY);
}

void sendJson(const JsonDocument& doc) {
  String payload;
  serializeJson(doc, payload);
  g_client.send(payload);
}

void sendHello() {
  JsonDocument doc;
  doc["type"] = "hello";
  doc["device_id"] = "esp32-s3-voice-test";
  doc["session_id"] = g_sessionId;
  sendJson(doc);
}

void sendTextStartSession() {
  JsonDocument doc;
  doc["type"] = "start_session";
  doc["device_id"] = "esp32-s3-voice-test";
  doc["session_id"] = g_sessionId;
  doc["text"] = kVoiceTestQuery;
  sendJson(doc);
}

void onMessageCallback(WebsocketsMessage message) {
  JsonDocument doc;
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

  if (strcmp(type, "tts_chunk") == 0) {
    g_ttsStreaming = true;
    playPcmChunk(String(static_cast<const char*>(doc["payload_b64"] | "")));
  } else if (strcmp(type, "tts_end") == 0) {
    g_ttsStreaming = false;
    Serial.println("TTS playback complete");
  }
}

bool connectWifi() {
  Serial.print("Connecting WiFi: ");
  Serial.println(kVoiceTestWifiSsid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(kVoiceTestWifiSsid, kVoiceTestWifiPassword);

  uint32_t startMs = millis();
  wl_status_t lastStatus = WL_IDLE_STATUS;

  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < kWifiConnectTimeoutMs) {
    wl_status_t status = WiFi.status();
    if (status != lastStatus) {
      Serial.print("WiFi status=");
      Serial.println(wifiStatusText(status));
      lastStatus = status;
    }
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("WiFi connect timeout, final status=");
    Serial.println(wifiStatusText(WiFi.status()));
    Serial.println("Hint: ESP32 usually cannot connect to 5GHz SSID. Use a 2.4GHz WiFi.");
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

  g_client.onMessage(onMessageCallback);
  g_wsReady = g_client.connect(url);
  Serial.println(g_wsReady ? "Backend WS connected" : "Backend WS connect failed");
  return g_wsReady;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("VOICE BACKEND TEXT TEST");

  if (strlen(kVoiceTestWifiSsid) == 0 || strlen(kVoiceTestWifiPassword) == 0) {
    Serial.println("Please fill firmware/include/voice_backend_test_config.h first");
    while (true) {
      delay(1000);
    }
  }

  g_sessionId = "sess-" + String(static_cast<uint32_t>(millis()));
  initSpeakerI2s();
  if (!connectWifi()) {
    return;
  }
  if (!connectBackend()) {
    return;
  }

  sendHello();
  delay(200);
  sendTextStartSession();
}

void loop() {
  if (g_wsReady) {
    g_client.poll();
  }
  delay(10);
}
