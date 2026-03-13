#pragma once

// Fill these before building the voice backend text test.
constexpr char kVoiceTestWifiSsid[] = "888";
constexpr char kVoiceTestWifiPassword[] = "888ye999";
constexpr char kVoiceTestBackendHost[] = "192.168.1.67";
constexpr uint16_t kVoiceTestBackendPort = 8787;
constexpr char kVoiceTestBackendPath[] = "/ws/device";
constexpr char kVoiceTestQuery[] =
    "请做一个简短自我介绍，并告诉我你是ESP32健康助手。";
