#pragma once

#include <Arduino.h>

#include "domain_types.h"

class Adafruit_SSD1306;
class MAX30105;
class Adafruit_MLX90614;

namespace websockets {
class WebsocketsClient;
}

namespace backend_log {
struct DeviceStateLogCache;
}

enum class SpeakerVolumePreset : uint8_t;

extern Adafruit_SSD1306 display;
extern MAX30105 max30102;
extern Adafruit_MLX90614 mlx90614;

extern VitalSigns g_vitals;
extern TemperatureData g_temperature;
extern SensorState g_sensorState;
extern uint32_t g_lastUiRefreshMs;
extern uint32_t g_lastSerialLogMs;
extern uint32_t g_lastTempReadMs;
extern uint32_t g_lastMlxRetryMs;
extern bool g_mlxReady;
extern uint8_t g_mlxAddrInUse;
extern uint32_t g_irBuffer[];
extern uint32_t g_redBuffer[];
extern size_t g_bufferCount;
extern size_t g_newSamplesSinceCalc;
extern float g_hrHistory[];
extern uint8_t g_hrHistoryIndex;
extern uint8_t g_hrHistoryCount;
extern float g_spo2History[];
extern uint8_t g_spo2HistoryIndex;
extern uint8_t g_spo2HistoryCount;
extern uint8_t g_hrValidStreak;
extern uint8_t g_hrInvalidStreak;
extern uint8_t g_spo2ValidStreak;
extern uint8_t g_spo2InvalidStreak;
extern uint8_t g_lowHrCandidateStreak;
extern uint32_t g_lastBeatMs;
extern uint32_t g_lastFingerSeenMs;
extern float g_currentPerfusionIndex;
extern uint8_t g_signalConfidence;
extern uint8_t g_ledAmplitude;
extern uint32_t g_lastLedTuneMs;
extern float g_calibratedPerfusionIndex;
extern uint32_t g_calibrationStartMs;
extern float g_calibrationPiSum;
extern uint8_t g_calibrationWindowCount;
extern bool g_mpuReady;
extern bool g_i2sReady;
extern bool g_micReady;
extern bool g_speakerActive;
extern uint8_t g_mpuAddr;
extern uint32_t g_lastMpuReadMs;
extern uint32_t g_freeFallMs;
extern uint32_t g_impactMs;
extern uint32_t g_impactCandidateMs;
extern uint32_t g_stillSinceMs;
extern uint32_t g_alertStartMs;
extern uint32_t g_cooldownUntilMs;
extern uint32_t g_nextAlarmMs;
extern FallState g_fallState;
extern MpuSample g_mpuSample;
extern SystemMode g_systemMode;
extern uint32_t g_pendingEvents;
extern uint32_t g_lastEventLogMs;
extern uint32_t g_lastEventMask;
extern bool g_lastFingerEventState;
extern bool g_sensorFaultLatched;
extern bool g_voiceActive;
extern bool g_voiceCalibrated;
extern float g_voiceNoiseFloor;
extern float g_voiceRms;
extern float g_voiceOnThreshold;
extern float g_voiceOffThreshold;
extern uint32_t g_voiceStartMs;
extern uint32_t g_voiceLastUpdateMs;
extern uint32_t g_voiceLastActiveMs;
extern uint32_t g_lastVoiceWakeMs;
extern uint8_t g_voiceOnFrameCount;
extern uint8_t g_voiceOffFrameCount;
extern VoiceState g_voiceState;
extern uint32_t g_voiceListenUntilMs;
extern FallProfile g_fallProfile;
extern uint32_t g_lastSerialCmdPollMs;
extern GuidanceHint g_guidanceHint;
extern websockets::WebsocketsClient g_backendClient;
extern bool g_backendWsReady;
extern bool g_backendHelloSent;
extern bool g_backendWifiStarted;
extern bool g_backendBridgeEnabled;
extern SpeakerVolumePreset g_runtimeSpeakerPreset;
extern float g_runtimeBackendTtsGain;
extern bool g_backendVoiceSessionActive;
extern bool g_backendWaitingForReply;
extern bool g_backendTtsStreaming;
extern bool g_backendInterruptArmed;
extern bool g_backendStopRequested;
extern LogProfile g_logProfile;
extern uint8_t g_backendInterruptFrames;
extern uint32_t g_backendVoiceSessionIdCounter;
extern uint32_t g_backendLastStreamMs;
extern uint32_t g_backendLastStatusPushMs;
extern uint32_t g_backendLastSpeakerRate;
extern String g_backendSessionId;
extern String g_backendReplyTextBuffer;
extern backend_log::DeviceStateLogCache g_backendStatusLogCache;
extern uint32_t g_backendWifiStartMs;
extern uint32_t g_lastBackendRetryMs;
extern bool g_pendingPromptTone;
extern uint8_t g_mpuReadFailStreak;
extern uint8_t g_micReadFailStreak;
extern uint8_t g_mlxReadFailStreak;
extern uint32_t g_i2cErrorCount;
extern uint32_t g_lastHealthLogMs;
extern uint32_t g_lastLoopMs;
extern uint32_t g_loopOverrunCount;
extern uint32_t g_lastLoopDurationUs;
extern uint32_t g_maxLoopDurationUs;
extern uint32_t g_lastMaxSampleMs;
extern uint8_t g_maxSampleStallCount;
extern float g_lastAlgoHrBpm;
extern bool g_lastAlgoHrValid;
extern uint32_t g_lastAlgoHrMs;
extern uint8_t g_spo2FallbackUseStreak;
