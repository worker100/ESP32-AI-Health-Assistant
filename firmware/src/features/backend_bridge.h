#pragma once

#include <stdint.h>

#include <WiFi.h>
#include <ArduinoWebsockets.h>

// 后端桥接层（backend bridge）：
// - Wi-Fi + WebSocket 连接管理
// - 语音 session stream/TTS 控制
// - 后端消息解析与状态同步

const char* backendWifiStatusText(wl_status_t status);
const char* backendVoiceStateUiText();
void onBackendMessage(websockets::WebsocketsMessage message);
void stopBackendBridge();
void updateBackendBridge(uint32_t now);
void updateBackendVoiceAssistant(uint32_t now);
