#pragma once

const char* backendWifiStatusText(wl_status_t status) {
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

const char* backendVoiceStateText() {
  if (g_backendTtsStreaming) {
    return "speaking";
  }
  if (g_backendWaitingForReply || g_backendVoiceSessionActive) {
    return "thinking";
  }
  if (!g_voiceCalibrated) {
    return "calibrating";
  }
  if (g_voiceState == VoiceState::Listen || g_voiceActive) {
    return "listening";
  }
  if (g_systemMode == SystemMode::Alerting) {
    return "alerting";
  }
  return "idle";
}

const char* backendVoiceStateUiText() {
  if (g_backendTtsStreaming) {
    return "SPK";
  }
  if (g_backendWaitingForReply || g_backendVoiceSessionActive) {
    return "THK";
  }
  if (!g_voiceCalibrated) {
    return "CAL";
  }
  if (g_voiceState == VoiceState::Listen || g_voiceActive) {
    return "LSN";
  }
  if (g_systemMode == SystemMode::Alerting) {
    return "ALR";
  }
  return "IDL";
}

bool encodeBackendBase64(const uint8_t* input, size_t inputLen, char* output, size_t outputCap) {
  return features::backend_codec::encodeBase64(input, inputLen, output, outputCap);
}

bool decodeBackendBase64(const String& input, uint8_t* output, size_t outputCap, size_t& outputLen) {
  return features::backend_codec::decodeBase64(input, output, outputCap, outputLen);
}

void ensureSpeakerSampleRate(uint32_t sampleRate) {
  if (!g_i2sReady || !g_speakerActive) {
    return;
  }
  if (g_backendLastSpeakerRate == sampleRate) {
    return;
  }
  i2s_set_clk(
      kI2sPort,
      sampleRate,
      static_cast<i2s_bits_per_sample_t>(kI2sBitsPerSample),
      I2S_CHANNEL_MONO);
  g_backendLastSpeakerRate = sampleRate;
}

void sendBackendJson(const JsonDocument& doc) {
  String payload;
  serializeJson(doc, payload);
  g_backendClient.send(payload);
}

void sendBackendHello() {
  JsonDocument doc;
  doc["type"] = "hello";
  doc["device_id"] = "esp32-ai-health-assistant-main";
  doc["session_id"] = "main-status";
  sendBackendJson(doc);
  g_backendHelloSent = true;
}

void sendBackendDeviceStatus(const AiHealthContextSnapshot& ctx);

void startBackendVoiceSession() {
  g_backendSessionId = "main-voice-" + String(++g_backendVoiceSessionIdCounter);
  // Push latest health snapshot before creating a new AI session so backend context
  // reflects the same moment the user sees on OLED/serial.
  sendBackendDeviceStatus(buildAiHealthContextSnapshot());
  JsonDocument doc;
  doc["type"] = "start_session";
  doc["device_id"] = "esp32-ai-health-assistant-main";
  doc["session_id"] = g_backendSessionId;
  sendBackendJson(doc);
}

void stopBackendVoiceSession() {
  JsonDocument doc;
  doc["type"] = "stop_session";
  doc["device_id"] = "esp32-ai-health-assistant-main";
  doc["session_id"] = g_backendSessionId;
  sendBackendJson(doc);
}

void interruptBackendVoiceSession() {
  JsonDocument doc;
  doc["type"] = "interrupt";
  doc["device_id"] = "esp32-ai-health-assistant-main";
  doc["session_id"] = g_backendSessionId;
  sendBackendJson(doc);
}

bool readBackendMicFrame(int16_t* outPcm, size_t samples, float& rmsNorm) {
  int32_t raw[kBackendMicFrameSamples] = {0};
  size_t bytesRead = 0;
  const esp_err_t err =
      i2s_read(kMicI2sPort, raw, samples * sizeof(int32_t), &bytesRead, pdMS_TO_TICKS(40));
  if (err != ESP_OK || bytesRead == 0) {
    return false;
  }

  const size_t count = bytesRead / sizeof(int32_t);
  if (count == 0) {
    return false;
  }

  double sumSq = 0.0;
  for (size_t i = 0; i < count; ++i) {
    const int32_t s24 = raw[i] >> 8;
    outPcm[i] = static_cast<int16_t>(s24 >> 8);
    sumSq += static_cast<double>(s24) * static_cast<double>(s24);
  }

  rmsNorm = sqrt(sumSq / count) / 8388608.0f;
  return true;
}

void sendBackendAudioChunk(const int16_t* pcm, size_t bytes) {
  char b64[kBackendMicBase64Cap] = {0};
  if (!encodeBackendBase64(reinterpret_cast<const uint8_t*>(pcm), bytes, b64, sizeof(b64))) {
    Serial.println("Backend audio base64 encode failed");
    return;
  }

  JsonDocument doc;
  doc["type"] = "audio_chunk";
  doc["device_id"] = "esp32-ai-health-assistant-main";
  doc["session_id"] = g_backendSessionId;
  doc["sample_rate"] = kMicSampleRate;
  doc["payload_b64"] = b64;
  sendBackendJson(doc);
}

void playBackendPcmChunk(const String& payloadB64) {
  uint8_t raw[kBackendTtsChunkBytes] = {0};
  size_t decodedLen = 0;
  if (!decodeBackendBase64(payloadB64, raw, sizeof(raw), decodedLen)) {
    Serial.println("Failed to decode backend tts_chunk");
    return;
  }
  int16_t* samples = reinterpret_cast<int16_t*>(raw);
  const size_t sampleCount = decodedLen / sizeof(int16_t);
  for (size_t i = 0; i < sampleCount; ++i) {
    int32_t scaled = static_cast<int32_t>(samples[i] * g_runtimeBackendTtsGain);
    if (scaled > 32767) {
      scaled = 32767;
    } else if (scaled < -32768) {
      scaled = -32768;
    }
    samples[i] = static_cast<int16_t>(scaled);
  }
  ensureSpeakerActive();
  ensureSpeakerSampleRate(24000);
  size_t written = 0;
  i2s_write(kI2sPort, raw, decodedLen, &written, portMAX_DELAY);
}

void sendBackendDeviceStatus(const AiHealthContextSnapshot& ctx) {
  JsonDocument doc;
  doc["type"] = "device_status";
  doc["device_id"] = "esp32-ai-health-assistant-main";
  doc["session_id"] = "main-status";
  doc["state"] = backendVoiceStateText();
  doc["fall_state"] = fallStateText(ctx.fallState);
  doc["signal_quality"] = qualityText(ctx.signalQuality);
  doc["finger_detected"] = ctx.fingerDetected;
  doc["device_source"] = "main_firmware";
  doc["temperature_source"] = ctx.tempSource;
  doc["measurement_confidence"] = ctx.measurementConfidence;
  doc["temperature_validity"] = ctx.temperatureValidity;
  // Keep display values in upload payload when available.
  // Reliability is still described by confidence/validity fields.
  const bool hasHrValue = isfinite(ctx.heartRateBpm) && ctx.heartRateBpm > 1.0f;
  const bool hasSpo2Value = isfinite(ctx.spo2Percent) && ctx.spo2Percent > 1.0f;
  const bool hasTempValue =
      isfinite(ctx.temperatureC) && ctx.temperatureC > -40.0f && ctx.temperatureC < 125.0f;

  if (ctx.heartRateValid || hasHrValue) {
    doc["heart_rate_bpm"] = ctx.heartRateBpm;
  }
  if (ctx.spo2Valid || hasSpo2Value) {
    doc["spo2_percent"] = ctx.spo2Percent;
  }
  if (ctx.temperatureValid || hasTempValue) {
    doc["temperature_c"] = ctx.temperatureC;
  }
  sendBackendJson(doc);
  g_backendLastStatusPushMs = millis();
}

void onBackendMessage(websockets::WebsocketsMessage message) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, message.data());
  if (err) {
    Serial.print("BJSON err=");
    Serial.println(err.c_str());
    return;
  }

  const char* type = doc["type"] | "";
  const char* detail = doc["detail"] | "";
  const char* text = doc["text"] | "";

  if (strcmp(type, "reply_text") == 0) {
    if (strlen(text) > 0) {
      g_backendReplyTextBuffer += text;
    }
    return;
  }

  if (strcmp(type, "request_fresh_status") == 0) {
    // Force one UI refresh before reporting, so OLED and backend context stay aligned.
    g_lastUiRefreshMs = 0;
    renderUi();
    sendBackendDeviceStatus(buildAiHealthContextSnapshot());
    Serial.println("BWS type=request_fresh_status -> status pushed");
    return;
  }

  if (strcmp(type, "session_started") == 0) {
    g_backendVoiceSessionActive = true;
    g_backendWaitingForReply = false;
    g_backendInterruptArmed = false;
    g_backendStopRequested = false;
    g_backendInterruptFrames = 0;
    g_backendReplyTextBuffer = "";
    Serial.println("BWS type=session_started");
    sendBackendDeviceStatus(buildAiHealthContextSnapshot());
    return;
  }

  if (strcmp(type, "backend_status") == 0) {
    if (backend_log::shouldPrintBackendStatus(isFullLogProfile(), detail, millis(),
                                              g_backendStatusLogCache)) {
      Serial.print("BWS type=backend_status");
      if (strlen(detail) > 0) {
        Serial.print(" detail=");
        Serial.print(detail);
      }
      Serial.println();
    }
    if (strcmp(detail, "interrupt_ack") == 0 || strcmp(detail, "session_stop_ack") == 0 ||
        strcmp(detail, "audio_chunk_ignored_no_live_session") == 0) {
      g_backendTtsStreaming = false;
      g_backendVoiceSessionActive = false;
      g_backendWaitingForReply = false;
      g_backendInterruptArmed = false;
      g_backendStopRequested = false;
      g_backendInterruptFrames = 0;
      g_backendReplyTextBuffer = "";
      Serial.println("Backend session cleared -> monitoring");
      sendBackendDeviceStatus(buildAiHealthContextSnapshot());
    }
    return;
  }

  if (strcmp(type, "error") == 0) {
    Serial.print("BWS type=error");
    if (strlen(detail) > 0) {
      Serial.print(" detail=");
      Serial.print(detail);
    }
    Serial.println();
    g_backendTtsStreaming = false;
    g_backendVoiceSessionActive = false;
    g_backendWaitingForReply = false;
    g_backendInterruptArmed = false;
    g_backendStopRequested = false;
    g_backendInterruptFrames = 0;
    g_backendReplyTextBuffer = "";
    i2s_zero_dma_buffer(kI2sPort);
    Serial.println("Backend error -> reset voice session state");
    sendBackendDeviceStatus(buildAiHealthContextSnapshot());
    return;
  }

  if (strcmp(type, "tts_chunk") == 0) {
    if (g_backendInterruptArmed) {
      return;
    }
    g_backendTtsStreaming = true;
    playBackendPcmChunk(String(static_cast<const char*>(doc["payload_b64"] | "")));
    return;
  }

  if (strcmp(type, "tts_end") == 0) {
    const char* roundText = doc["text"] | "";
    if (strlen(roundText) > 0) {
      g_backendReplyTextBuffer = String(roundText);
    }
    g_backendTtsStreaming = false;
    g_backendVoiceSessionActive = false;
    g_backendWaitingForReply = false;
    g_backendInterruptArmed = false;
    g_backendStopRequested = false;
    g_backendInterruptFrames = 0;
    Serial.print("BWS type=tts_end");
    if (strlen(detail) > 0) {
      Serial.print(" detail=");
      Serial.print(detail);
    }
    Serial.println();
    if (g_backendReplyTextBuffer.length() > 0) {
      Serial.print("AI=");
      Serial.println(g_backendReplyTextBuffer);
    }
    g_backendReplyTextBuffer = "";
    Serial.println("Backend voice round complete");
    sendBackendDeviceStatus(buildAiHealthContextSnapshot());
    return;
  }

  Serial.print("BWS type=");
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
}

bool connectBackendSocket() {
  String url = "ws://";
  url += kProjectBackendHost;
  url += ":";
  url += String(kProjectBackendPort);
  url += kProjectBackendPath;

  Serial.print("Backend connect: ");
  Serial.println(url);
  g_backendWsReady = g_backendClient.connect(url);
  if (g_backendWsReady) {
    Serial.println("Backend WS connected (main)");
    g_backendHelloSent = false;
  } else {
    Serial.println("Backend WS connect failed (main)");
  }
  return g_backendWsReady;
}

void stopBackendBridge() {
  g_backendBridgeEnabled = false;
  if (g_backendClient.available()) {
    g_backendClient.close();
  }
  g_backendWsReady = false;
  g_backendHelloSent = false;
  g_backendWifiStarted = false;
  g_backendWifiStartMs = 0;
  g_lastBackendRetryMs = 0;
}

void updateBackendBridge(uint32_t now) {
  if (!g_backendBridgeEnabled) {
    return;
  }

  if (!g_backendWifiStarted) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(kProjectWifiSsid, kProjectWifiPassword);
    g_backendWifiStarted = true;
    g_backendWifiStartMs = now;
    Serial.print("Backend WiFi connecting: ");
    Serial.println(kProjectWifiSsid);
    return;
  }

  const wl_status_t wifiStatus = WiFi.status();
  if (wifiStatus != WL_CONNECTED) {
    if ((now - g_backendWifiStartMs) > kBackendWifiConnectTimeoutMs &&
        (now - g_lastBackendRetryMs) > kBackendRetryMs) {
      g_lastBackendRetryMs = now;
      g_backendWifiStartMs = now;
      WiFi.disconnect();
      WiFi.begin(kProjectWifiSsid, kProjectWifiPassword);
      Serial.print("Backend WiFi retry, status=");
      Serial.println(backendWifiStatusText(wifiStatus));
    }
    g_backendWsReady = false;
    g_backendHelloSent = false;
    return;
  }

  if (!g_backendWsReady) {
    if ((now - g_lastBackendRetryMs) >= kBackendRetryMs) {
      g_lastBackendRetryMs = now;
      connectBackendSocket();
    }
    return;
  }

  g_backendClient.poll();
  if (!g_backendClient.available()) {
    g_backendWsReady = false;
    g_backendHelloSent = false;
    Serial.println("Backend WS disconnected (main)");
    return;
  }

  if (!g_backendHelloSent) {
    sendBackendHello();
  }

  if ((now - g_backendLastStatusPushMs) >= kBackendStatusPushMs) {
    const AiHealthContextSnapshot ctx = buildAiHealthContextSnapshot();
    sendBackendDeviceStatus(ctx);
  }
}

void updateBackendVoiceAssistant(uint32_t now) {
  if (!g_backendBridgeEnabled || !g_backendWsReady || !g_micReady) {
    return;
  }

  if (g_systemMode == SystemMode::Alerting) {
    if (g_backendTtsStreaming || g_backendVoiceSessionActive || g_backendWaitingForReply) {
      g_backendInterruptArmed = true;
      g_backendTtsStreaming = false;
      g_backendVoiceSessionActive = false;
      g_backendWaitingForReply = false;
      g_backendStopRequested = false;
      g_backendInterruptFrames = 0;
      i2s_zero_dma_buffer(kI2sPort);
      interruptBackendVoiceSession();
      sendBackendDeviceStatus(buildAiHealthContextSnapshot());
    }
    return;
  }

  const bool needMicFrame =
      g_backendTtsStreaming || g_backendVoiceSessionActive ||
      (g_voiceState == VoiceState::Listen);
  if (!needMicFrame) {
    return;
  }

  int16_t pcm[kBackendMicFrameSamples] = {0};
  float rmsNorm = 0.0f;
  if (!readBackendMicFrame(pcm, kBackendMicFrameSamples, rmsNorm)) {
    return;
  }

  const float interruptOnTh =
      std::max(kBackendInterruptMinRms, g_voiceNoiseFloor * kBackendInterruptOnFactor);

  if (g_backendTtsStreaming) {
    if (rmsNorm >= interruptOnTh) {
      if (g_backendInterruptFrames < 255) {
        ++g_backendInterruptFrames;
      }
    } else {
      g_backendInterruptFrames = 0;
    }

    if (!g_backendInterruptArmed && g_backendInterruptFrames >= kBackendInterruptTriggerFrames) {
      g_backendInterruptArmed = true;
      g_backendTtsStreaming = false;
      g_backendVoiceSessionActive = false;
      g_backendWaitingForReply = false;
      g_backendStopRequested = false;
      g_backendInterruptFrames = 0;
      i2s_zero_dma_buffer(kI2sPort);
      interruptBackendVoiceSession();
      Serial.println("Main TTS interrupted -> ready");
      sendBackendDeviceStatus(buildAiHealthContextSnapshot());
    }
    return;
  }

  if (g_voiceState == VoiceState::Listen && !g_backendVoiceSessionActive && !g_backendWaitingForReply) {
    startBackendVoiceSession();
    g_backendWaitingForReply = true;
    g_backendStopRequested = false;
    g_backendLastStreamMs = now;
    Serial.println("Main VOICE_ON -> backend start session");
    sendBackendDeviceStatus(buildAiHealthContextSnapshot());
    return;
  }

  if (g_backendVoiceSessionActive) {
    sendBackendAudioChunk(pcm, kBackendMicChunkBytes);
    g_backendLastStreamMs = now;
  }

  if (!g_backendStopRequested &&
      (g_backendVoiceSessionActive || g_backendWaitingForReply) &&
      g_voiceState != VoiceState::Listen &&
      (now - g_backendLastStreamMs) > kBackendTailStreamMs) {
    stopBackendVoiceSession();
    g_backendWaitingForReply = true;
    g_backendVoiceSessionActive = false;
    g_backendStopRequested = true;
    Serial.println("Main VOICE_OFF -> backend stop session");
    sendBackendDeviceStatus(buildAiHealthContextSnapshot());
  }
}

