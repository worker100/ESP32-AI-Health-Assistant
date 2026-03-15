#include "app_state.h"

#include <ArduinoWebsockets.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>
#include "../drivers/max30105_compat.h"

#include "runtime_constants.h"
#include "../modules/backend_log.h"
#include "project_config.h"

Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
MAX30105 max30102;
Adafruit_MLX90614 mlx90614;

VitalSigns g_vitals;
TemperatureData g_temperature;
SensorState g_sensorState = SensorState::WaitingFinger;
uint32_t g_lastUiRefreshMs = 0;
uint32_t g_lastSerialLogMs = 0;
uint32_t g_lastTempReadMs = 0;
uint32_t g_lastMlxRetryMs = 0;
bool g_mlxReady = false;
uint8_t g_mlxAddrInUse = 0;
uint32_t g_irBuffer[kAlgoWindowSize] = {0};
uint32_t g_redBuffer[kAlgoWindowSize] = {0};
size_t g_bufferCount = 0;
size_t g_newSamplesSinceCalc = 0;
float g_hrHistory[kMedianWindowSize] = {0};
uint8_t g_hrHistoryIndex = 0;
uint8_t g_hrHistoryCount = 0;
float g_spo2History[kMedianWindowSize] = {0};
uint8_t g_spo2HistoryIndex = 0;
uint8_t g_spo2HistoryCount = 0;
uint8_t g_hrValidStreak = 0;
uint8_t g_hrInvalidStreak = 0;
uint8_t g_spo2ValidStreak = 0;
uint8_t g_spo2InvalidStreak = 0;
uint8_t g_lowHrCandidateStreak = 0;
uint32_t g_lastBeatMs = 0;
uint32_t g_lastFingerSeenMs = 0;
float g_currentPerfusionIndex = 0.0f;
uint8_t g_signalConfidence = 0;
uint8_t g_ledAmplitude = 0x1F;
uint32_t g_lastLedTuneMs = 0;
float g_calibratedPerfusionIndex = kMinPerfusionIndex;
uint32_t g_calibrationStartMs = 0;
float g_calibrationPiSum = 0.0f;
uint8_t g_calibrationWindowCount = 0;
bool g_mpuReady = false;
bool g_i2sReady = false;
bool g_micReady = false;
bool g_speakerActive = false;
uint8_t g_mpuAddr = 0;
uint32_t g_lastMpuReadMs = 0;
uint32_t g_freeFallMs = 0;
uint32_t g_impactMs = 0;
uint32_t g_impactCandidateMs = 0;
uint32_t g_stillSinceMs = 0;
uint32_t g_alertStartMs = 0;
uint32_t g_cooldownUntilMs = 0;
uint32_t g_nextAlarmMs = 0;
FallState g_fallState = FallState::Normal;
MpuSample g_mpuSample;
SystemMode g_systemMode = SystemMode::Booting;
uint32_t g_pendingEvents = 0;
uint32_t g_lastEventLogMs = 0;
uint32_t g_lastEventMask = 0;
bool g_lastFingerEventState = false;
bool g_sensorFaultLatched = false;
bool g_voiceActive = false;
bool g_voiceCalibrated = false;
float g_voiceNoiseFloor = 0.0f;
float g_voiceRms = 0.0f;
float g_voiceOnThreshold = 0.0f;
float g_voiceOffThreshold = 0.0f;
uint32_t g_voiceStartMs = 0;
uint32_t g_voiceLastUpdateMs = 0;
uint32_t g_voiceLastActiveMs = 0;
uint32_t g_lastVoiceWakeMs = 0;
uint8_t g_voiceOnFrameCount = 0;
uint8_t g_voiceOffFrameCount = 0;
VoiceState g_voiceState = VoiceState::Idle;
uint32_t g_voiceListenUntilMs = 0;
FallProfile g_fallProfile = FallProfile::Normal;
uint32_t g_lastSerialCmdPollMs = 0;
GuidanceHint g_guidanceHint = GuidanceHint::None;
websockets::WebsocketsClient g_backendClient;
bool g_backendWsReady = false;
bool g_backendHelloSent = false;
bool g_backendWifiStarted = false;
bool g_backendBridgeEnabled = kProjectBackendBridgeDefaultEnabled;
SpeakerVolumePreset g_runtimeSpeakerPreset = kSpeakerVolumePreset;
float g_runtimeBackendTtsGain = kBackendTtsGain;
bool g_backendVoiceSessionActive = false;
bool g_backendWaitingForReply = false;
bool g_backendTtsStreaming = false;
bool g_backendInterruptArmed = false;
bool g_backendStopRequested = false;
LogProfile g_logProfile = LogProfile::Demo;
uint8_t g_backendInterruptFrames = 0;
uint32_t g_backendVoiceSessionIdCounter = 0;
uint32_t g_backendLastStreamMs = 0;
uint32_t g_backendLastStatusPushMs = 0;
uint32_t g_backendLastSpeakerRate = 0;
String g_backendSessionId = "main-status";
String g_backendReplyTextBuffer;
backend_log::DeviceStateLogCache g_backendStatusLogCache;
uint32_t g_backendWifiStartMs = 0;
uint32_t g_lastBackendRetryMs = 0;
bool g_pendingPromptTone = false;
uint8_t g_mpuReadFailStreak = 0;
uint8_t g_micReadFailStreak = 0;
uint8_t g_mlxReadFailStreak = 0;
uint32_t g_i2cErrorCount = 0;
uint32_t g_lastHealthLogMs = 0;
uint32_t g_lastLoopMs = 0;
uint32_t g_loopOverrunCount = 0;
uint32_t g_lastLoopDurationUs = 0;
uint32_t g_maxLoopDurationUs = 0;
uint32_t g_lastMaxSampleMs = 0;
uint8_t g_maxSampleStallCount = 0;
float g_lastAlgoHrBpm = 0.0f;
bool g_lastAlgoHrValid = false;
uint32_t g_lastAlgoHrMs = 0;
uint8_t g_spo2FallbackUseStreak = 0;
