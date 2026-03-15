#pragma once

#include <stdint.h>

// 项目级默认参数（project defaults）。
// 可在 config/project_local.h 通过 AI_HEALTH_* macro 覆盖。
//
// 参数速查（quick map）：
// - 网络/后端：AI_HEALTH_WIFI_* / AI_HEALTH_BACKEND_*
// - 音量：AI_HEALTH_SPEAKER_PRESET + AI_HEALTH_SPK_GAIN_*
// - VAD/后端阈值：AI_HEALTH_MIC_* / AI_HEALTH_BACKEND_*
// - HR/SpO2 显示约束：AI_HEALTH_*_HOLD_MS / AI_HEALTH_MIN/MAX_ACCEPTED_*

#ifndef AI_HEALTH_WIFI_SSID
#define AI_HEALTH_WIFI_SSID "888"
#endif

#ifndef AI_HEALTH_WIFI_PASSWORD
#define AI_HEALTH_WIFI_PASSWORD "888ye999"
#endif

#ifndef AI_HEALTH_BACKEND_HOST
#define AI_HEALTH_BACKEND_HOST "192.168.1.67"
#endif

#ifndef AI_HEALTH_BACKEND_PORT
#define AI_HEALTH_BACKEND_PORT 8787
#endif

#ifndef AI_HEALTH_BACKEND_PATH
#define AI_HEALTH_BACKEND_PATH "/ws/device"
#endif

#ifndef AI_HEALTH_BACKEND_BRIDGE_DEFAULT_ENABLED
#define AI_HEALTH_BACKEND_BRIDGE_DEFAULT_ENABLED 1
#endif

constexpr char kProjectWifiSsid[] = AI_HEALTH_WIFI_SSID;
constexpr char kProjectWifiPassword[] = AI_HEALTH_WIFI_PASSWORD;
constexpr char kProjectBackendHost[] = AI_HEALTH_BACKEND_HOST;
constexpr uint16_t kProjectBackendPort = static_cast<uint16_t>(AI_HEALTH_BACKEND_PORT);
constexpr char kProjectBackendPath[] = AI_HEALTH_BACKEND_PATH;
constexpr bool kProjectBackendBridgeDefaultEnabled =
    (AI_HEALTH_BACKEND_BRIDGE_DEFAULT_ENABLED != 0);

// ===== Speaker / TTS 音量 =====
// 预设映射（preset mapping）：
//   0 = Low，1 = Medium，2 = High
// 持久默认值：设置 AI_HEALTH_SPEAKER_PRESET。
// 运行时临时覆盖：串口命令 `VOL LOW | VOL MED | VOL HIGH`。
enum class SpeakerVolumePreset : uint8_t {
  Low = 0,
  Medium = 1,
  High = 2,
};

#ifndef AI_HEALTH_SPK_GAIN_LOW
#define AI_HEALTH_SPK_GAIN_LOW 0.55f
#endif

#ifndef AI_HEALTH_SPK_GAIN_MEDIUM
#define AI_HEALTH_SPK_GAIN_MEDIUM 1.25f
#endif

#ifndef AI_HEALTH_SPK_GAIN_HIGH
#define AI_HEALTH_SPK_GAIN_HIGH 1.65f
#endif

constexpr float kSpeakerGainLow = AI_HEALTH_SPK_GAIN_LOW;
constexpr float kSpeakerGainMedium = AI_HEALTH_SPK_GAIN_MEDIUM;
constexpr float kSpeakerGainHigh = AI_HEALTH_SPK_GAIN_HIGH;

#ifndef AI_HEALTH_SPEAKER_PRESET
#define AI_HEALTH_SPEAKER_PRESET 1
#endif

constexpr SpeakerVolumePreset kSpeakerVolumePreset =
    static_cast<SpeakerVolumePreset>(AI_HEALTH_SPEAKER_PRESET);

constexpr float speakerGainFromPreset(SpeakerVolumePreset preset) {
  return (preset == SpeakerVolumePreset::Low)
             ? kSpeakerGainLow
             : ((preset == SpeakerVolumePreset::High) ? kSpeakerGainHigh : kSpeakerGainMedium);
}

constexpr float kBackendTtsGain = speakerGainFromPreset(kSpeakerVolumePreset);

// ===== Backend bridge 频率与中断阈值 =====
#ifndef AI_HEALTH_BACKEND_STATUS_PUSH_MS
#define AI_HEALTH_BACKEND_STATUS_PUSH_MS 800
#endif

#ifndef AI_HEALTH_BACKEND_RETRY_MS
#define AI_HEALTH_BACKEND_RETRY_MS 5000
#endif

#ifndef AI_HEALTH_BACKEND_WIFI_TIMEOUT_MS
#define AI_HEALTH_BACKEND_WIFI_TIMEOUT_MS 20000
#endif

#ifndef AI_HEALTH_BACKEND_INTERRUPT_ON_FACTOR
#define AI_HEALTH_BACKEND_INTERRUPT_ON_FACTOR 3.4f
#endif

#ifndef AI_HEALTH_BACKEND_INTERRUPT_MIN_RMS
#define AI_HEALTH_BACKEND_INTERRUPT_MIN_RMS 0.015f
#endif

#ifndef AI_HEALTH_BACKEND_INTERRUPT_TRIGGER_FRAMES
#define AI_HEALTH_BACKEND_INTERRUPT_TRIGGER_FRAMES 2
#endif

#ifndef AI_HEALTH_BACKEND_TAIL_STREAM_MS
#define AI_HEALTH_BACKEND_TAIL_STREAM_MS 350
#endif

constexpr uint32_t kBackendStatusPushMs = AI_HEALTH_BACKEND_STATUS_PUSH_MS;
constexpr uint32_t kBackendRetryMs = AI_HEALTH_BACKEND_RETRY_MS;
constexpr uint32_t kBackendWifiConnectTimeoutMs = AI_HEALTH_BACKEND_WIFI_TIMEOUT_MS;
constexpr float kBackendInterruptOnFactor = AI_HEALTH_BACKEND_INTERRUPT_ON_FACTOR;
constexpr float kBackendInterruptMinRms = AI_HEALTH_BACKEND_INTERRUPT_MIN_RMS;
constexpr uint8_t kBackendInterruptTriggerFrames = AI_HEALTH_BACKEND_INTERRUPT_TRIGGER_FRAMES;
constexpr uint32_t kBackendTailStreamMs = AI_HEALTH_BACKEND_TAIL_STREAM_MS;

// ===== Microphone VAD 参数 =====
#ifndef AI_HEALTH_MIC_VAD_CAL_MS
#define AI_HEALTH_MIC_VAD_CAL_MS 3000
#endif

#ifndef AI_HEALTH_MIC_VAD_NOISE_ALPHA
#define AI_HEALTH_MIC_VAD_NOISE_ALPHA 0.08f
#endif

#ifndef AI_HEALTH_MIC_VAD_ON_FACTOR
#define AI_HEALTH_MIC_VAD_ON_FACTOR 2.8f
#endif

#ifndef AI_HEALTH_MIC_VAD_OFF_FACTOR
#define AI_HEALTH_MIC_VAD_OFF_FACTOR 1.8f
#endif

#ifndef AI_HEALTH_MIC_VAD_MIN_ON
#define AI_HEALTH_MIC_VAD_MIN_ON 0.012f
#endif

#ifndef AI_HEALTH_MIC_VAD_MIN_OFF
#define AI_HEALTH_MIC_VAD_MIN_OFF 0.0065f
#endif

#ifndef AI_HEALTH_MIC_VAD_OFF_HOLD_MS
#define AI_HEALTH_MIC_VAD_OFF_HOLD_MS 450
#endif

#ifndef AI_HEALTH_MIC_VAD_MIN_TRIGGER_INTERVAL_MS
#define AI_HEALTH_MIC_VAD_MIN_TRIGGER_INTERVAL_MS 1200
#endif

#ifndef AI_HEALTH_MIC_VAD_CONSECUTIVE_ON_FRAMES
#define AI_HEALTH_MIC_VAD_CONSECUTIVE_ON_FRAMES 2
#endif

#ifndef AI_HEALTH_MIC_VAD_CONSECUTIVE_OFF_FRAMES
#define AI_HEALTH_MIC_VAD_CONSECUTIVE_OFF_FRAMES 2
#endif

#ifndef AI_HEALTH_VOICE_LISTEN_WINDOW_MS
#define AI_HEALTH_VOICE_LISTEN_WINDOW_MS 4500
#endif

constexpr uint32_t kMicVadCalMs = AI_HEALTH_MIC_VAD_CAL_MS;
constexpr float kMicVadNoiseAlpha = AI_HEALTH_MIC_VAD_NOISE_ALPHA;
constexpr float kMicVadOnFactor = AI_HEALTH_MIC_VAD_ON_FACTOR;
constexpr float kMicVadOffFactor = AI_HEALTH_MIC_VAD_OFF_FACTOR;
constexpr float kMicVadMinOn = AI_HEALTH_MIC_VAD_MIN_ON;
constexpr float kMicVadMinOff = AI_HEALTH_MIC_VAD_MIN_OFF;
constexpr uint32_t kMicVadOffHoldMs = AI_HEALTH_MIC_VAD_OFF_HOLD_MS;
constexpr uint32_t kMicVadMinTriggerIntervalMs = AI_HEALTH_MIC_VAD_MIN_TRIGGER_INTERVAL_MS;
constexpr uint8_t kMicVadConsecutiveOnFrames = AI_HEALTH_MIC_VAD_CONSECUTIVE_ON_FRAMES;
constexpr uint8_t kMicVadConsecutiveOffFrames = AI_HEALTH_MIC_VAD_CONSECUTIVE_OFF_FRAMES;
constexpr uint32_t kVoiceListenWindowMs = AI_HEALTH_VOICE_LISTEN_WINDOW_MS;

// ===== 健康显示与有效值范围（acceptance bounds） =====
#ifndef AI_HEALTH_HR_DISPLAY_HOLD_MS
#define AI_HEALTH_HR_DISPLAY_HOLD_MS 15000
#endif

#ifndef AI_HEALTH_SPO2_DISPLAY_HOLD_MS
#define AI_HEALTH_SPO2_DISPLAY_HOLD_MS 25000
#endif

#ifndef AI_HEALTH_HR_DISPLAY_STABLE_WINDOW_MS
#define AI_HEALTH_HR_DISPLAY_STABLE_WINDOW_MS 900
#endif

#ifndef AI_HEALTH_SPO2_DISPLAY_STABLE_WINDOW_MS
#define AI_HEALTH_SPO2_DISPLAY_STABLE_WINDOW_MS 1200
#endif

#ifndef AI_HEALTH_MIN_ACCEPTED_BPM
#define AI_HEALTH_MIN_ACCEPTED_BPM 45.0f
#endif

#ifndef AI_HEALTH_MAX_ACCEPTED_BPM
#define AI_HEALTH_MAX_ACCEPTED_BPM 125.0f
#endif

#ifndef AI_HEALTH_MIN_ACCEPTED_SPO2
#define AI_HEALTH_MIN_ACCEPTED_SPO2 82
#endif

#ifndef AI_HEALTH_MAX_ACCEPTED_SPO2
#define AI_HEALTH_MAX_ACCEPTED_SPO2 100
#endif

constexpr uint32_t kHrDisplayHoldMs = AI_HEALTH_HR_DISPLAY_HOLD_MS;
constexpr uint32_t kSpo2DisplayHoldMs = AI_HEALTH_SPO2_DISPLAY_HOLD_MS;
constexpr uint32_t kHrDisplayStableWindowMs = AI_HEALTH_HR_DISPLAY_STABLE_WINDOW_MS;
constexpr uint32_t kSpo2DisplayStableWindowMs = AI_HEALTH_SPO2_DISPLAY_STABLE_WINDOW_MS;
constexpr float kMinAcceptedBpm = AI_HEALTH_MIN_ACCEPTED_BPM;
constexpr float kMaxAcceptedBpm = AI_HEALTH_MAX_ACCEPTED_BPM;
constexpr int32_t kMinAcceptedSpo2 = AI_HEALTH_MIN_ACCEPTED_SPO2;
constexpr int32_t kMaxAcceptedSpo2 = AI_HEALTH_MAX_ACCEPTED_SPO2;
