#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MLX90614.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"
#include <driver/i2s.h>
#include <algorithm>
#include <math.h>
#include <string.h>

namespace {
constexpr uint8_t kI2cSda = 8;
constexpr uint8_t kI2cScl = 9;
constexpr uint8_t kOledAddr = 0x3C;
constexpr uint8_t kMax30102Addr = 0x57;
constexpr uint8_t kMlxDefaultAddr = 0x5A;
constexpr bool kEnableMlx90614 = true;
constexpr bool kEnableMax30102DieTemp = true;
constexpr uint32_t kMax30102TempReadMs = 2000;
constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr uint32_t kUiRefreshMs = 250;
constexpr uint32_t kSerialLogMs = 1000;
constexpr uint32_t kTempReadMs = 1000;
constexpr uint32_t kMlxRetryMs = 5000;
constexpr uint32_t kBeatTimeoutMs = 12000;
constexpr uint32_t kCalibrationDurationMs = 3000;
constexpr uint32_t kFingerLostDebounceMs = 1800;
constexpr uint32_t kHrDisplayHoldMs = 15000;
constexpr uint32_t kSpo2DisplayHoldMs = 25000;
constexpr float kHrDisplayAlpha = 0.35f;
constexpr float kHrRealtimeMaxJump = 10.0f;
constexpr float kSpo2DisplayAlpha = 0.32f;
constexpr float kSpo2RealtimeMaxJump = 2.5f;
constexpr float kMinAcceptedBpm = 45.0f;
constexpr float kMaxAcceptedBpm = 125.0f;
constexpr int32_t kMinAcceptedSpo2 = 82;
constexpr int32_t kMaxAcceptedSpo2 = 100;
constexpr size_t kAlgoWindowSize = 100;
constexpr size_t kAlgoStepSamples = 25;
constexpr float kHrEwmaAlpha = 0.30f;
constexpr float kHrAlgoFallbackAlpha = 0.22f;
constexpr float kSpo2EwmaAlpha = 0.25f;
constexpr uint8_t kMedianWindowSize = 5;
constexpr float kMaxHrJumpPerUpdate = 16.0f;
constexpr float kMaxSpo2JumpPerUpdate = 5.5f;
constexpr float kMinPerfusionIndex = 0.0012f;
constexpr uint8_t kHrValidStreakRequired = 2;
constexpr uint8_t kSpo2ValidStreakRequired = 1;
constexpr uint8_t kInvalidStreakDrop = 28;
constexpr float kMaxSpo2ReacquireJump = 7.0f;
constexpr float kSpo2MinQualityRatio = 0.08f;
constexpr float kMaxSpo2FallbackJumpPerUpdate = 4.8f;
constexpr uint32_t kSpo2MinIrDc = 12000;
constexpr uint32_t kSpo2MinRedDc = 7000;
constexpr float kSpo2MinAcP2P = 60.0f;
constexpr uint8_t kCalibrationMinWindows = 1;
constexpr uint32_t kBeatRealtimeStaleMs = 2500;
constexpr float kAlgoHrMinQualityRatio = 0.28f;
constexpr float kAlgoHrMaxDeltaFromDisplay = 15.0f;
constexpr float kLowHrGuardFloorBpm = 52.0f;
constexpr uint8_t kLowHrGuardMinConfidence = 40;
constexpr uint8_t kLowHrGuardConsecutiveRequired = 3;
constexpr long kFingerDetectThreshold = 50000;
constexpr uint32_t kIrTargetLow = 75000;
constexpr uint32_t kIrTargetHigh = 150000;
constexpr uint8_t kLedAmplitudeMin = 0x08;
constexpr uint8_t kLedAmplitudeMax = 0x5F;
constexpr uint8_t kLedAmplitudeStep = 2;
constexpr uint32_t kLedTuneIntervalMs = 1000;
constexpr uint8_t kMpuAddr0 = 0x68;
constexpr uint8_t kMpuAddr1 = 0x69;
constexpr uint8_t kMpuWhoAmIReg = 0x75;
constexpr uint8_t kMpuPwrMgmt1Reg = 0x6B;
constexpr uint8_t kMpuAccelConfigReg = 0x1C;
constexpr uint8_t kMpuGyroConfigReg = 0x1B;
constexpr uint8_t kMpuAccelStartReg = 0x3B;
constexpr uint32_t kMpuReadMs = 100;
constexpr uint32_t kAlarmContinuousMs = 12000;
constexpr uint32_t kAlarmMonitorMs = 30000;
constexpr uint32_t kImpactWindowMsNormal = 1300;
constexpr uint32_t kStillWindowMsNormal = 700;
constexpr uint32_t kImpactWindowMsTest = 900;
constexpr uint32_t kStillWindowMsTest = 550;
constexpr uint32_t kFallCooldownMs = 3000;
constexpr uint32_t kAlertBurstIntervalMs = 900;
constexpr uint32_t kMonitorBurstIntervalMs = 3000;
constexpr uint32_t kAlertBurstIntervalMsTest = 700;
constexpr uint32_t kMonitorBurstIntervalMsTest = 2200;
constexpr float kAlarmToneHighHz = 1320.0f;
constexpr float kAlarmToneMidHz = 900.0f;
constexpr float kPromptToneHz = 660.0f;
constexpr float kFreeFallThresholdGNormal = 0.80f;
constexpr float kImpactThresholdGNormal = 0.95f;
constexpr float kImpactRotationThresholdDpsNormal = 20.0f;
constexpr float kFreeFallThresholdGTest = 0.72f;
constexpr float kImpactThresholdGTest = 0.90f;
constexpr float kImpactRotationThresholdDpsTest = 16.0f;
constexpr float kStillAccelMinG = 0.85f;
constexpr float kStillAccelMaxG = 1.15f;
constexpr float kStillGyroThresholdDps = 40.0f;
constexpr float kRecoveryAccelThresholdG = 1.55f;
constexpr float kRecoveryGyroThresholdDps = 110.0f;
constexpr uint32_t kImpactConfirmMinMs = 80;
constexpr uint32_t kStillConfirmMinMs = 180;
constexpr uint32_t kAggressiveMotionGuardMs = 220;
constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr int kI2sSampleRate = 16000;
constexpr int kI2sBclkPin = 4;
constexpr int kI2sLrcPin = 5;
constexpr int kI2sDinPin = 6;
constexpr int kI2sBitsPerSample = 16;
constexpr size_t kI2sAudioBufferSamples = 128;
constexpr bool kEnableMicVad = true;
constexpr i2s_port_t kMicI2sPort = I2S_NUM_1;
constexpr int kMicSampleRate = 16000;
constexpr int kMicBclkPin = 15;
constexpr int kMicWsPin = 16;
constexpr int kMicDinPin = 17;
constexpr size_t kMicVadSamples = 256;
constexpr uint32_t kMicVadUpdateMs = 120;
constexpr uint32_t kMicVadCalMs = 3000;
constexpr float kMicVadNoiseAlpha = 0.08f;
constexpr float kMicVadOnFactor = 2.8f;
constexpr float kMicVadOffFactor = 1.8f;
constexpr float kMicVadMinOn = 0.012f;
constexpr float kMicVadMinOff = 0.0065f;
constexpr uint32_t kMicVadOffHoldMs = 450;
constexpr uint32_t kMicVadMinTriggerIntervalMs = 1200;
constexpr uint8_t kMicVadConsecutiveOnFrames = 2;
constexpr uint8_t kMicVadConsecutiveOffFrames = 2;
constexpr uint32_t kVoiceListenWindowMs = 4500;
constexpr uint32_t kSerialCmdPollMs = 120;
constexpr uint32_t kHealthLogMs = 5000;
constexpr uint8_t kMpuReadFailReinitThreshold = 6;
constexpr uint8_t kMicReadFailReinitThreshold = 10;
constexpr uint8_t kMlxReadFailReinitThreshold = 6;
constexpr uint32_t kMaxSampleStallMs = 4000;
constexpr uint8_t kMaxSampleStallReinitThreshold = 2;

enum class SensorState {
  WaitingFinger,
  Calibrating,
  Running,
};

enum class FallState {
  Normal,
  FreeFall,
  Impact,
  Alert,
  Monitor,
};

enum class SignalQuality {
  NoFinger,
  Waiting,
  Calibrating,
  Poor,
  Fair,
  Good,
};

enum class SystemMode {
  Booting,
  Monitoring,
  Alerting,
  Degraded,
};

enum class FallProfile {
  Normal,
  Test,
};

enum class VoiceState {
  Idle,
  Listen,
};

enum class GuidanceHint {
  None,
  NoFinger,
  LowPerfusion,
  Unstable,
  Spo2Weak,
};

constexpr uint32_t kEvtFallFreeFall = 1UL << 0;
constexpr uint32_t kEvtFallImpact = 1UL << 1;
constexpr uint32_t kEvtFallAlert = 1UL << 2;
constexpr uint32_t kEvtFallCleared = 1UL << 3;
constexpr uint32_t kEvtNoFinger = 1UL << 4;
constexpr uint32_t kEvtFingerDetected = 1UL << 5;
constexpr uint32_t kEvtSensorError = 1UL << 6;
constexpr uint32_t kEvtSensorRecovered = 1UL << 7;
constexpr uint32_t kEvtVoiceWakeReserved = 1UL << 8;
constexpr uint32_t kEvtVoiceListenStart = 1UL << 9;
constexpr uint32_t kEvtVoiceListenEnd = 1UL << 10;
constexpr uint32_t kEvtFallProfileChanged = 1UL << 11;

Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
MAX30105 max30102;
Adafruit_MLX90614 mlx90614;

struct VitalSigns {
  float heartRateBpm = 0.0f;
  float spo2Percent = 0.0f;
  float heartRateDisplayBpm = 0.0f;
  float heartRateRealtimeBpm = 0.0f;
  float spo2DisplayPercent = 0.0f;
  float spo2RealtimePercent = 0.0f;
  bool heartRateValid = false;
  bool spo2Valid = false;
  bool heartRateDisplayValid = false;
  bool heartRateRealtimeValid = false;
  bool spo2DisplayValid = false;
  bool spo2RealtimeValid = false;
  bool fingerDetected = false;
  uint32_t lastHrUpdateMs = 0;
  uint32_t lastSpo2UpdateMs = 0;
  uint32_t lastHrBeatAcceptedMs = 0;
  uint32_t lastHrDisplayRefreshMs = 0;
  uint32_t lastSpo2DisplayRefreshMs = 0;
  uint32_t irValue = 0;
  uint32_t redValue = 0;
};

struct TemperatureData {
  float ambientC = 0.0f;
  float objectC = 0.0f;
  bool valid = false;
  bool fromMax30102Die = false;
  uint32_t lastUpdateMs = 0;
};

struct MpuSample {
  float axG = 0.0f;
  float ayG = 0.0f;
  float azG = 0.0f;
  float gxDps = 0.0f;
  float gyDps = 0.0f;
  float gzDps = 0.0f;
  float accelMagG = 0.0f;
  float gyroMaxDps = 0.0f;
  bool valid = false;
};

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

float medianFromHistory(const float* history, uint8_t count) {
  if (count == 0) {
    return 0.0f;
  }

  float scratch[kMedianWindowSize] = {0};
  for (uint8_t i = 0; i < count; ++i) {
    scratch[i] = history[i];
  }
  std::sort(scratch, scratch + count);

  if ((count & 1U) == 1U) {
    return scratch[count / 2];
  }
  return (scratch[(count / 2) - 1] + scratch[count / 2]) * 0.5f;
}

float pushAndMedian(float value, float* history, uint8_t* index, uint8_t* count) {
  history[*index] = value;
  *index = static_cast<uint8_t>((*index + 1) % kMedianWindowSize);
  if (*count < kMedianWindowSize) {
    *count += 1;
  }
  return medianFromHistory(history, *count);
}

void resetMeasurementFilters() {
  g_bufferCount = 0;
  g_newSamplesSinceCalc = 0;
  g_hrHistoryIndex = 0;
  g_hrHistoryCount = 0;
  g_spo2HistoryIndex = 0;
  g_spo2HistoryCount = 0;
  g_hrValidStreak = 0;
  g_hrInvalidStreak = 0;
  g_spo2ValidStreak = 0;
  g_spo2InvalidStreak = 0;
  g_lowHrCandidateStreak = 0;
  g_lastBeatMs = 0;
  g_currentPerfusionIndex = 0.0f;
  g_vitals.heartRateBpm = 0.0f;
  g_vitals.spo2Percent = 0.0f;
  g_vitals.heartRateDisplayBpm = 0.0f;
  g_vitals.heartRateRealtimeBpm = 0.0f;
  g_vitals.spo2DisplayPercent = 0.0f;
  g_vitals.spo2RealtimePercent = 0.0f;
  g_vitals.heartRateValid = false;
  g_vitals.spo2Valid = false;
  g_vitals.heartRateDisplayValid = false;
  g_vitals.heartRateRealtimeValid = false;
  g_vitals.spo2DisplayValid = false;
  g_vitals.spo2RealtimeValid = false;
  g_vitals.lastHrBeatAcceptedMs = 0;
  g_vitals.lastHrDisplayRefreshMs = 0;
  g_vitals.lastSpo2DisplayRefreshMs = 0;
}

void startCalibration(uint32_t now) {
  g_sensorState = SensorState::Calibrating;
  g_calibrationStartMs = now;
  g_calibrationPiSum = 0.0f;
  g_calibrationWindowCount = 0;
  resetMeasurementFilters();
}

const char* sensorStateText() {
  switch (g_sensorState) {
    case SensorState::WaitingFinger:
      return "WAIT";
    case SensorState::Calibrating:
      return "CAL";
    case SensorState::Running:
      return "RUN";
  }
  return "UNK";
}

SignalQuality currentSignalQuality() {
  if (!g_vitals.fingerDetected) {
    return SignalQuality::NoFinger;
  }
  if (g_sensorState == SensorState::WaitingFinger) {
    return SignalQuality::Waiting;
  }
  if (g_sensorState == SensorState::Calibrating) {
    return SignalQuality::Calibrating;
  }

  const float baseline = std::max(g_calibratedPerfusionIndex, kMinPerfusionIndex);
  const float ratio = g_currentPerfusionIndex / baseline;
  if (g_vitals.heartRateValid && g_vitals.spo2Valid && ratio >= 0.75f) {
    return SignalQuality::Good;
  }
  if ((g_vitals.heartRateValid || g_vitals.spo2Valid) && ratio >= 0.35f) {
    return SignalQuality::Fair;
  }
  return SignalQuality::Poor;
}

const char* qualityText(SignalQuality quality) {
  switch (quality) {
    case SignalQuality::NoFinger:
      return "NO FINGER";
    case SignalQuality::Waiting:
      return "WAITING";
    case SignalQuality::Calibrating:
      return "CALIBRATING";
    case SignalQuality::Poor:
      return "POOR";
    case SignalQuality::Fair:
      return "FAIR";
    case SignalQuality::Good:
      return "GOOD";
  }
  return "UNKNOWN";
}

uint8_t clampConfidence(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 100) {
    return 100;
  }
  return static_cast<uint8_t>(value);
}

uint8_t calcSignalConfidence(float qualityRatio, bool hrValid, bool spo2Valid) {
  int score = 0;
  score += static_cast<int>(std::max(0.0f, std::min(1.0f, (qualityRatio - 0.20f) / 0.80f)) * 60.0f);
  if (hrValid) {
    score += 20;
  }
  if (spo2Valid) {
    score += 20;
  }
  return clampConfidence(score);
}

GuidanceHint evaluateGuidance(float qualityRatio, float perfusionIndex, bool fingerDetected,
                              float accelMag, float spo2Candidate) {
  if (!fingerDetected) {
    return GuidanceHint::NoFinger;
  }
  if (qualityRatio < 0.22f || perfusionIndex < (kMinPerfusionIndex * 0.8f)) {
    return GuidanceHint::LowPerfusion;
  }
  if (fabsf(accelMag - 1.0f) > 0.18f) {
    return GuidanceHint::Unstable;
  }
  if (spo2Candidate > 0.0f && spo2Candidate < 90.0f && qualityRatio < 0.35f) {
    return GuidanceHint::Spo2Weak;
  }
  return GuidanceHint::None;
}

void autoTuneLedAmplitude(uint32_t irDc, uint32_t now) {
  if (!g_vitals.fingerDetected) {
    return;
  }
  if (now - g_lastLedTuneMs < kLedTuneIntervalMs) {
    return;
  }
  g_lastLedTuneMs = now;

  uint8_t nextAmp = g_ledAmplitude;
  if (irDc < kIrTargetLow && g_ledAmplitude < kLedAmplitudeMax) {
    const uint16_t raised = static_cast<uint16_t>(g_ledAmplitude) + kLedAmplitudeStep;
    nextAmp = static_cast<uint8_t>(std::min<uint16_t>(raised, kLedAmplitudeMax));
  } else if (irDc > kIrTargetHigh && g_ledAmplitude > kLedAmplitudeMin) {
    const int lowered = static_cast<int>(g_ledAmplitude) - static_cast<int>(kLedAmplitudeStep);
    nextAmp = static_cast<uint8_t>(std::max(lowered, static_cast<int>(kLedAmplitudeMin)));
  }

  if (nextAmp != g_ledAmplitude) {
    g_ledAmplitude = nextAmp;
    max30102.setPulseAmplitudeRed(g_ledAmplitude);
    max30102.setPulseAmplitudeIR(g_ledAmplitude);
    Serial.print("LED auto-tune amp=0x");
    if (g_ledAmplitude < 16) {
      Serial.print('0');
    }
    Serial.println(g_ledAmplitude, HEX);
  }
}

bool estimateSpo2FromWindowFallback(uint32_t irDc, float irAcP2P, uint32_t redDc, float redAcP2P,
                                    float* spo2Out) {
  if (irDc < kSpo2MinIrDc || redDc < kSpo2MinRedDc || irAcP2P < kSpo2MinAcP2P ||
      redAcP2P < kSpo2MinAcP2P) {
    return false;
  }

  const float irNorm = irAcP2P / static_cast<float>(irDc);
  const float redNorm = redAcP2P / static_cast<float>(redDc);
  if (irNorm <= 1e-6f) {
    return false;
  }

  const float ratio = redNorm / irNorm;
  if (!isfinite(ratio) || ratio <= 0.0f || ratio > 4.0f) {
    return false;
  }

  const float estimated = -45.060f * ratio * ratio + 30.354f * ratio + 94.845f;
  if (!isfinite(estimated) || estimated < 80.0f || estimated > 100.0f) {
    return false;
  }

  if (spo2Out != nullptr) {
    *spo2Out = estimated;
  }
  return true;
}

const char* fallStateText(FallState state) {
  switch (state) {
    case FallState::Normal:
      return "NORMAL";
    case FallState::FreeFall:
      return "FREEFALL";
    case FallState::Impact:
      return "IMPACT";
    case FallState::Alert:
      return "ALERT";
    case FallState::Monitor:
      return "MONITOR";
  }
  return "UNKNOWN";
}

const char* fallStateUiText(FallState state) {
  switch (state) {
    case FallState::Normal:
      return "NRM";
    case FallState::FreeFall:
      return "DRP";
    case FallState::Impact:
      return "HIT";
    case FallState::Alert:
      return "ALR";
    case FallState::Monitor:
      return "MON";
  }
  return "UNK";
}

void publishEvent(uint32_t eventBit) {
  g_pendingEvents |= eventBit;
}

const char* systemModeText(SystemMode mode) {
  switch (mode) {
    case SystemMode::Booting:
      return "BOOT";
    case SystemMode::Monitoring:
      return "MON";
    case SystemMode::Alerting:
      return "ALERT";
    case SystemMode::Degraded:
      return "DEG";
  }
  return "UNK";
}

const char* fallProfileText(FallProfile profile) {
  return (profile == FallProfile::Test) ? "TEST" : "NORMAL";
}

const char* voiceStateText(VoiceState state) {
  return (state == VoiceState::Listen) ? "LISTEN" : "IDLE";
}

const char* guidanceText(GuidanceHint hint) {
  switch (hint) {
    case GuidanceHint::None:
      return "NONE";
    case GuidanceHint::NoFinger:
      return "NO_FINGER";
    case GuidanceHint::LowPerfusion:
      return "LOW_PI";
    case GuidanceHint::Unstable:
      return "UNSTABLE";
    case GuidanceHint::Spo2Weak:
      return "SPO2_WEAK";
  }
  return "UNK";
}

float activeFreeFallThresholdG() {
  return (g_fallProfile == FallProfile::Test) ? kFreeFallThresholdGTest : kFreeFallThresholdGNormal;
}

float activeImpactThresholdG() {
  return (g_fallProfile == FallProfile::Test) ? kImpactThresholdGTest : kImpactThresholdGNormal;
}

float activeImpactRotationThresholdDps() {
  return (g_fallProfile == FallProfile::Test) ? kImpactRotationThresholdDpsTest
                                              : kImpactRotationThresholdDpsNormal;
}

uint32_t activeImpactWindowMs() {
  return (g_fallProfile == FallProfile::Test) ? kImpactWindowMsTest : kImpactWindowMsNormal;
}

uint32_t activeStillWindowMs() {
  return (g_fallProfile == FallProfile::Test) ? kStillWindowMsTest : kStillWindowMsNormal;
}

void updateSystemModeAndEvents(uint32_t now) {
  const bool fallActive = (g_fallState == FallState::Alert || g_fallState == FallState::Monitor);
  const bool sensorFault = !g_mpuReady || !g_i2sReady || (kEnableMicVad && !g_micReady);

  if (sensorFault && !g_sensorFaultLatched) {
    g_sensorFaultLatched = true;
    publishEvent(kEvtSensorError);
  } else if (!sensorFault && g_sensorFaultLatched) {
    g_sensorFaultLatched = false;
    publishEvent(kEvtSensorRecovered);
  }

  if (fallActive) {
    g_systemMode = SystemMode::Alerting;
  } else if (sensorFault) {
    g_systemMode = SystemMode::Degraded;
  } else {
    g_systemMode = SystemMode::Monitoring;
  }

  if (g_pendingEvents == 0 || (now - g_lastEventLogMs) < 120) {
    return;
  }

  g_lastEventLogMs = now;
  g_lastEventMask = g_pendingEvents;
  Serial.print("EVT:");
  if (g_pendingEvents & kEvtFallFreeFall) {
    Serial.print(" FREEFALL");
  }
  if (g_pendingEvents & kEvtFallImpact) {
    Serial.print(" IMPACT");
  }
  if (g_pendingEvents & kEvtFallAlert) {
    Serial.print(" FALL_ALERT");
  }
  if (g_pendingEvents & kEvtFallCleared) {
    Serial.print(" FALL_CLEARED");
  }
  if (g_pendingEvents & kEvtFingerDetected) {
    Serial.print(" FINGER_ON");
  }
  if (g_pendingEvents & kEvtNoFinger) {
    Serial.print(" FINGER_OFF");
  }
  if (g_pendingEvents & kEvtSensorError) {
    Serial.print(" SENSOR_ERR");
  }
  if (g_pendingEvents & kEvtSensorRecovered) {
    Serial.print(" SENSOR_OK");
  }
  if (g_pendingEvents & kEvtVoiceWakeReserved) {
    Serial.print(" VOICE_WAKE");
  }
  if (g_pendingEvents & kEvtVoiceListenStart) {
    Serial.print(" VOICE_LISTEN_START");
  }
  if (g_pendingEvents & kEvtVoiceListenEnd) {
    Serial.print(" VOICE_LISTEN_END");
  }
  if (g_pendingEvents & kEvtFallProfileChanged) {
    Serial.print(" FALL_PROFILE_CHANGED");
  }
  Serial.println();
  g_pendingEvents = 0;
}

void printFallThresholds() {
  const float ff = activeFreeFallThresholdG();
  const float imp = activeImpactThresholdG();
  const float gthr = activeImpactRotationThresholdDps();
  const uint32_t impWin = activeImpactWindowMs();
  const uint32_t stillWin = activeStillWindowMs();

  Serial.println("Fall thresholds:");
  Serial.print("  PROFILE: ");
  Serial.println(fallProfileText(g_fallProfile));
  Serial.print("  FREEFALL: M < ");
  Serial.print(ff, 2);
  Serial.println("g");
  Serial.print("  IMPACT: M > ");
  Serial.print(imp, 2);
  Serial.print("g and Gmax > ");
  Serial.print(gthr, 0);
  Serial.println(" dps");
  Serial.print("  IMPACT window: ");
  Serial.print(impWin);
  Serial.println(" ms");
  Serial.print("  STILL: M in [");
  Serial.print(kStillAccelMinG, 2);
  Serial.print(", ");
  Serial.print(kStillAccelMaxG, 2);
  Serial.print("] g and Gmax < ");
  Serial.print(kStillGyroThresholdDps, 0);
  Serial.println(" dps");
  Serial.print("  STILL window: ");
  Serial.print(stillWin);
  Serial.println(" ms");
}

void setFallProfile(FallProfile profile, bool announce = true) {
  if (g_fallProfile == profile) {
    return;
  }
  g_fallProfile = profile;
  publishEvent(kEvtFallProfileChanged);
  if (announce) {
    Serial.print("Fall profile set to ");
    Serial.println(fallProfileText(g_fallProfile));
    printFallThresholds();
  }
  g_pendingPromptTone = true;
}

void pollSerialCommands(uint32_t now) {
  if ((now - g_lastSerialCmdPollMs) < kSerialCmdPollMs || !Serial.available()) {
    return;
  }
  g_lastSerialCmdPollMs = now;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "MODE TEST" || cmd == "FALL TEST") {
    setFallProfile(FallProfile::Test);
  } else if (cmd == "MODE NORMAL" || cmd == "FALL NORMAL") {
    setFallProfile(FallProfile::Normal);
  } else if (cmd == "MODE?" || cmd == "FALL?") {
    Serial.print("Fall profile=");
    Serial.println(fallProfileText(g_fallProfile));
  } else if (cmd == "HELP") {
    Serial.println("Commands: MODE TEST | MODE NORMAL | MODE? | FALL? | HEALTH?");
  } else if (cmd == "HEALTH?") {
    Serial.print("Health i2cErr=");
    Serial.print(g_i2cErrorCount);
    Serial.print(" mpuFail=");
    Serial.print(g_mpuReadFailStreak);
    Serial.print(" micFail=");
    Serial.print(g_micReadFailStreak);
    Serial.print(" mlxFail=");
    Serial.print(g_mlxReadFailStreak);
    Serial.print(" loopUs=");
    Serial.print(g_lastLoopDurationUs);
    Serial.print(" loopUsMax=");
    Serial.print(g_maxLoopDurationUs);
    Serial.print(" overrun=");
    Serial.println(g_loopOverrunCount);
  }
}

void updateHealthSnapshot(uint32_t now, uint32_t loopStartUs) {
  const uint32_t duration = micros() - loopStartUs;
  g_lastLoopDurationUs = duration;
  g_lastLoopMs = now;
  if (duration > g_maxLoopDurationUs) {
    g_maxLoopDurationUs = duration;
  }
  if (duration > 20000U) {
    ++g_loopOverrunCount;
  }

  if ((now - g_lastHealthLogMs) < kHealthLogMs) {
    return;
  }
  g_lastHealthLogMs = now;
  Serial.print("HLTH loopUs=");
  Serial.print(g_lastLoopDurationUs);
  Serial.print(" maxUs=");
  Serial.print(g_maxLoopDurationUs);
  Serial.print(" i2cErr=");
  Serial.print(g_i2cErrorCount);
  Serial.print(" mpuFail=");
  Serial.print(g_mpuReadFailStreak);
  Serial.print(" mlxFail=");
  Serial.print(g_mlxReadFailStreak);
  Serial.print(" micFail=");
  Serial.print(g_micReadFailStreak);
  Serial.print(" overrun=");
  Serial.println(g_loopOverrunCount);
}

const char* qualityUiText(SignalQuality quality) {
  switch (quality) {
    case SignalQuality::NoFinger:
      return "NO";
    case SignalQuality::Waiting:
      return "WAIT";
    case SignalQuality::Calibrating:
      return "CAL";
    case SignalQuality::Poor:
      return "POOR";
    case SignalQuality::Fair:
      return "FAIR";
    case SignalQuality::Good:
      return "GOOD";
  }
  return "UNK";
}

const char* guidanceUiText(GuidanceHint hint) {
  switch (hint) {
    case GuidanceHint::None:
      return "OK";
    case GuidanceHint::NoFinger:
      return "FNG";
    case GuidanceHint::LowPerfusion:
      return "PI";
    case GuidanceHint::Unstable:
      return "MOV";
    case GuidanceHint::Spo2Weak:
      return "O2";
  }
  return "UNK";
}

void fatalError(const char* msg) {
  Serial.println(msg);
  if (display.width() > 0) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("AI Health Monitor");
    display.println("Init Failed:");
    display.println(msg);
    display.display();
  }
  while (true) {
    delay(500);
  }
}

void initI2c() {
  Wire.begin(kI2cSda, kI2cScl);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
}

const char* i2cErrorText(uint8_t err) {
  switch (err) {
    case 0:
      return "OK";
    case 1:
      return "DATA_TOO_LONG";
    case 2:
      return "ADDR_NACK";
    case 3:
      return "DATA_NACK";
    case 4:
      return "OTHER";
    case 5:
      return "TIMEOUT";
    default:
      return "UNKNOWN";
  }
}

bool i2cProbe(uint8_t addr, uint8_t* errOut = nullptr) {
  Wire.beginTransmission(addr);
  const uint8_t err = Wire.endTransmission();
  if (errOut != nullptr) {
    *errOut = err;
  }
  if (err != 0) {
    ++g_i2cErrorCount;
  }
  return err == 0;
}

bool mpuWriteReg8(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool mpuReadBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    ++g_i2cErrorCount;
    return false;
  }

  const size_t got = Wire.requestFrom(static_cast<int>(addr), static_cast<int>(len),
                                      static_cast<int>(true));
  if (got != len) {
    ++g_i2cErrorCount;
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buf[i] = Wire.read();
  }
  return true;
}

bool mpuReadReg8(uint8_t addr, uint8_t reg, uint8_t* valueOut) {
  return mpuReadBytes(addr, reg, valueOut, 1);
}

int16_t mpuReadInt16(const uint8_t* buf, size_t offset) {
  return static_cast<int16_t>((static_cast<uint16_t>(buf[offset]) << 8) | buf[offset + 1]);
}

bool mlxReadWordRaw(uint8_t addr, uint8_t reg, uint16_t* valueOut) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t received = Wire.requestFrom(static_cast<int>(addr), 3, static_cast<int>(true));
  if (received != 3) {
    return false;
  }

  const uint8_t low = Wire.read();
  const uint8_t high = Wire.read();
  (void)Wire.read();  // PEC byte; currently used only for probing, ignore here.

  if (valueOut != nullptr) {
    *valueOut = static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8);
  }
  return true;
}

bool mlxLooksReadableAt(uint8_t addr, float* objectTempOut = nullptr, float* ambientTempOut = nullptr) {
  uint16_t rawObj = 0;
  uint16_t rawAmb = 0;
  if (!mlxReadWordRaw(addr, 0x07, &rawObj) || !mlxReadWordRaw(addr, 0x06, &rawAmb)) {
    return false;
  }

  const float obj = static_cast<float>(rawObj) * 0.02f - 273.15f;
  const float amb = static_cast<float>(rawAmb) * 0.02f - 273.15f;

  if (!isfinite(obj) || !isfinite(amb)) {
    return false;
  }
  if (obj < -70.0f || obj > 400.0f || amb < -70.0f || amb > 200.0f) {
    return false;
  }

  if (objectTempOut != nullptr) {
    *objectTempOut = obj;
  }
  if (ambientTempOut != nullptr) {
    *ambientTempOut = amb;
  }
  return true;
}

void i2cScan() {
  Serial.println("I2C scan start...");
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("I2C device found: 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
    }
    delay(1);
  }
  Serial.println("I2C scan done.");
}

void initOled() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, kOledAddr)) {
    fatalError("OLED init failed");
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("AI Health Monitor");
  display.println("OLED initialized");
  display.display();
  delay(800);
  Serial.println("OLED initialized");
}

void initMax30102() {
  if (!max30102.begin(Wire, I2C_SPEED_STANDARD, kMax30102Addr)) {
    fatalError("MAX30102 init failed");
  }

  const byte ledBrightness = 60;
  const byte sampleAverage = 4;
  const byte ledMode = 2;
  const int sampleRate = 100;
  const int pulseWidth = 411;
  const int adcRange = 4096;

  max30102.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  g_ledAmplitude = 0x1F;
  max30102.setPulseAmplitudeRed(g_ledAmplitude);
  max30102.setPulseAmplitudeIR(g_ledAmplitude);
  max30102.setPulseAmplitudeGreen(0);
  g_lastMaxSampleMs = millis();
  g_maxSampleStallCount = 0;

  Serial.println("MAX30102 initialized");
}

bool initMpuAt(uint8_t addr) {
  uint8_t who = 0;
  if (!i2cProbe(addr) || !mpuReadReg8(addr, kMpuWhoAmIReg, &who)) {
    return false;
  }

  Serial.print("MPU probe 0x");
  if (addr < 16) {
    Serial.print('0');
  }
  Serial.print(addr, HEX);
  Serial.print(" WHO_AM_I=0x");
  if (who < 16) {
    Serial.print('0');
  }
  Serial.println(who, HEX);

  if (who != 0x68 && who != 0x69) {
    return false;
  }

  if (!mpuWriteReg8(addr, kMpuPwrMgmt1Reg, 0x00)) {
    return false;
  }
  delay(50);

  if (!mpuWriteReg8(addr, kMpuAccelConfigReg, 0x00)) {
    return false;
  }
  if (!mpuWriteReg8(addr, kMpuGyroConfigReg, 0x00)) {
    return false;
  }

  g_mpuReady = true;
  g_mpuAddr = addr;
  return true;
}

void initMpu6050() {
  g_mpuReady = false;
  g_mpuAddr = 0;
  g_mpuSample = {};

  if (initMpuAt(kMpuAddr0)) {
    Serial.println("MPU6050 initialized at 0x68");
    return;
  }

  if (initMpuAt(kMpuAddr1)) {
    Serial.println("MPU6050 initialized at 0x69");
    return;
  }

  Serial.println("MPU6050 init failed on 0x68/0x69");
}

void initI2sAudio() {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = kI2sSampleRate,
      .bits_per_sample = static_cast<i2s_bits_per_sample_t>(kI2sBitsPerSample),
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
      .bck_io_num = kI2sBclkPin,
      .ws_io_num = kI2sLrcPin,
      .data_out_num = kI2sDinPin,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };

  i2s_driver_uninstall(kI2sPort);
  if (i2s_driver_install(kI2sPort, &config, 0, nullptr) == ESP_OK &&
      i2s_set_pin(kI2sPort, &pins) == ESP_OK) {
    i2s_zero_dma_buffer(kI2sPort);
    i2s_stop(kI2sPort);
    g_i2sReady = true;
    g_speakerActive = false;
    Serial.println("MAX98357 I2S init OK");
    Serial.println("Speaker muted by default (non-alert mode)");
  } else {
    g_i2sReady = false;
    g_speakerActive = false;
    Serial.println("MAX98357 I2S init failed");
  }
}

void forceSpeakerPinsLow() {
  pinMode(kI2sBclkPin, OUTPUT);
  pinMode(kI2sLrcPin, OUTPUT);
  pinMode(kI2sDinPin, OUTPUT);
  digitalWrite(kI2sBclkPin, LOW);
  digitalWrite(kI2sLrcPin, LOW);
  digitalWrite(kI2sDinPin, LOW);
}

void ensureSpeakerActive() {
  if (!g_i2sReady || g_speakerActive) {
    return;
  }

  const i2s_pin_config_t pins = {
      .bck_io_num = kI2sBclkPin,
      .ws_io_num = kI2sLrcPin,
      .data_out_num = kI2sDinPin,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };
  i2s_set_pin(kI2sPort, &pins);
  i2s_zero_dma_buffer(kI2sPort);
  i2s_start(kI2sPort);
  g_speakerActive = true;
}

void ensureSpeakerMuted() {
  if (!g_i2sReady || !g_speakerActive) {
    return;
  }
  i2s_zero_dma_buffer(kI2sPort);
  i2s_stop(kI2sPort);
  forceSpeakerPinsLow();
  g_speakerActive = false;
}

void initMicI2s() {
  g_micReady = false;
  g_voiceActive = false;
  g_voiceCalibrated = false;
  g_voiceNoiseFloor = 0.0f;
  g_voiceRms = 0.0f;
  g_voiceOnThreshold = 0.0f;
  g_voiceOffThreshold = 0.0f;
  g_voiceStartMs = millis();
  g_voiceLastUpdateMs = 0;
  g_voiceLastActiveMs = 0;

  if (!kEnableMicVad) {
    return;
  }

  const i2s_config_t cfg = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = kMicSampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };

  const i2s_pin_config_t pins = {
      .bck_io_num = kMicBclkPin,
      .ws_io_num = kMicWsPin,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = kMicDinPin,
  };

  i2s_driver_uninstall(kMicI2sPort);
  if (i2s_driver_install(kMicI2sPort, &cfg, 0, nullptr) == ESP_OK &&
      i2s_set_pin(kMicI2sPort, &pins) == ESP_OK) {
    i2s_zero_dma_buffer(kMicI2sPort);
    g_micReady = true;
    Serial.println("INMP441 VAD I2S init OK");
  } else {
    Serial.println("INMP441 VAD I2S init failed");
  }
}

void updateVoiceVad(uint32_t now) {
  if (!kEnableMicVad || !g_micReady || (now - g_voiceLastUpdateMs) < kMicVadUpdateMs) {
    return;
  }
  g_voiceLastUpdateMs = now;

  int32_t raw[kMicVadSamples] = {0};
  size_t bytesRead = 0;
  const esp_err_t err =
      i2s_read(kMicI2sPort, raw, sizeof(raw), &bytesRead, pdMS_TO_TICKS(12));
  if (err != ESP_OK || bytesRead == 0) {
    if (g_micReadFailStreak < 255) {
      ++g_micReadFailStreak;
    }
    if (g_micReadFailStreak >= kMicReadFailReinitThreshold) {
      Serial.println("Mic VAD read fail streak, reinit...");
      initMicI2s();
      g_micReadFailStreak = 0;
    }
    return;
  }
  g_micReadFailStreak = 0;

  const size_t samples = bytesRead / sizeof(int32_t);
  if (samples == 0) {
    return;
  }

  double sumSq = 0.0;
  for (size_t i = 0; i < samples; ++i) {
    const int32_t s24 = raw[i] >> 8;
    sumSq += static_cast<double>(s24) * static_cast<double>(s24);
  }
  g_voiceRms = sqrtf(sumSq / static_cast<float>(samples)) / 8388608.0f;

  if (!g_voiceCalibrated) {
    if (g_voiceNoiseFloor <= 0.0f) {
      g_voiceNoiseFloor = g_voiceRms;
    } else {
      g_voiceNoiseFloor =
          (1.0f - kMicVadNoiseAlpha) * g_voiceNoiseFloor + kMicVadNoiseAlpha * g_voiceRms;
    }
    if ((now - g_voiceStartMs) >= kMicVadCalMs) {
      g_voiceCalibrated = true;
    }
  } else if (!g_voiceActive) {
    g_voiceNoiseFloor =
        (1.0f - kMicVadNoiseAlpha) * g_voiceNoiseFloor + kMicVadNoiseAlpha * g_voiceRms;
  }

  g_voiceOnThreshold = std::max(kMicVadMinOn, g_voiceNoiseFloor * kMicVadOnFactor);
  g_voiceOffThreshold = std::max(kMicVadMinOff, g_voiceNoiseFloor * kMicVadOffFactor);

  const bool aboveOn = g_voiceCalibrated && (g_voiceRms >= g_voiceOnThreshold);
  const bool belowOff = g_voiceRms < g_voiceOffThreshold;
  if (aboveOn) {
    if (g_voiceOnFrameCount < 255) {
      ++g_voiceOnFrameCount;
    }
    g_voiceOffFrameCount = 0;
  } else if (belowOff) {
    if (g_voiceOffFrameCount < 255) {
      ++g_voiceOffFrameCount;
    }
    g_voiceOnFrameCount = 0;
  }

  if (!g_voiceActive && g_voiceOnFrameCount >= kMicVadConsecutiveOnFrames) {
    g_voiceActive = true;
    g_voiceLastActiveMs = now;
    if ((now - g_lastVoiceWakeMs) >= kMicVadMinTriggerIntervalMs) {
      g_lastVoiceWakeMs = now;
      publishEvent(kEvtVoiceWakeReserved);
      if (g_voiceState == VoiceState::Idle) {
        g_voiceState = VoiceState::Listen;
        g_voiceListenUntilMs = now + kVoiceListenWindowMs;
        publishEvent(kEvtVoiceListenStart);
      }
    }
  } else if (g_voiceActive) {
    if (!belowOff) {
      g_voiceLastActiveMs = now;
      g_voiceOffFrameCount = 0;
    } else if (g_voiceOffFrameCount >= kMicVadConsecutiveOffFrames &&
               now - g_voiceLastActiveMs > kMicVadOffHoldMs) {
      g_voiceActive = false;
    }
  }

  if (g_voiceState == VoiceState::Listen && now >= g_voiceListenUntilMs) {
    g_voiceState = VoiceState::Idle;
    publishEvent(kEvtVoiceListenEnd);
  }
}

void playTone(float frequencyHz, uint16_t durationMs, float amplitude = 0.35f) {
  if (!g_i2sReady || !g_speakerActive) {
    return;
  }

  int16_t samples[kI2sAudioBufferSamples] = {0};
  const float omega = 2.0f * PI * frequencyHz / static_cast<float>(kI2sSampleRate);
  const uint32_t totalSamples =
      static_cast<uint32_t>(kI2sSampleRate) * static_cast<uint32_t>(durationMs) / 1000U;
  uint32_t produced = 0;

  while (produced < totalSamples) {
    const size_t chunk = std::min<size_t>(kI2sAudioBufferSamples, totalSamples - produced);
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
  if (!g_i2sReady || !g_speakerActive) {
    delay(durationMs);
    return;
  }

  int16_t samples[kI2sAudioBufferSamples] = {0};
  const uint32_t totalSamples =
      static_cast<uint32_t>(kI2sSampleRate) * static_cast<uint32_t>(durationMs) / 1000U;
  uint32_t produced = 0;

  while (produced < totalSamples) {
    const size_t chunk = std::min<size_t>(kI2sAudioBufferSamples, totalSamples - produced);
    size_t written = 0;
    i2s_write(kI2sPort, samples, chunk * sizeof(int16_t), &written, portMAX_DELAY);
    produced += chunk;
  }
}

void playAlarmBurst(bool monitorPhase) {
  const bool testProfile = (g_fallProfile == FallProfile::Test);
  const float first = monitorPhase ? kAlarmToneMidHz : (testProfile ? 1100.0f : kAlarmToneMidHz);
  const float second = monitorPhase ? (kAlarmToneMidHz + 260.0f) : kAlarmToneHighHz;
  const uint16_t t1 = monitorPhase ? 120 : (testProfile ? 140 : 160);
  const uint16_t t2 = monitorPhase ? 140 : (testProfile ? 160 : 180);
  playTone(first, t1);
  playSilence(60);
  playTone(second, t2);
  playSilence(testProfile ? 90 : 120);
}

void playPromptTone() {
  playTone(kPromptToneHz, 90, 0.22f);
}

void initMlx90614(bool verbose = true) {
  static const uint8_t kAddrCandidates[] = {0x5A, 0x5B, 0x5C, 0x5D};
  g_mlxReady = false;
  g_mlxAddrInUse = 0;
  g_temperature.valid = false;

  if (verbose) {
    Serial.println("MLX90614 probe start...");
  }

  for (uint8_t addr : kAddrCandidates) {
    uint8_t probeErr = 0;
    const bool ack = i2cProbe(addr, &probeErr);
    if (verbose) {
      Serial.print("  Probe 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.print(addr, HEX);
      Serial.print(": ");
      Serial.println(i2cErrorText(probeErr));
    }
    if (!ack) {
      continue;
    }

    float obj = NAN;
    float amb = NAN;
    if (mlxLooksReadableAt(addr, &obj, &amb)) {
      if (verbose) {
        Serial.print("    Raw read OK, Obj=");
        Serial.print(obj, 2);
        Serial.print("C, Amb=");
        Serial.print(amb, 2);
        Serial.println("C");
      }
    } else if (verbose) {
      Serial.println("    Raw read failed at this address.");
    }

    if (mlx90614.begin(addr, &Wire)) {
      g_mlxReady = true;
      g_mlxAddrInUse = addr;
      Serial.print("MLX90614 initialized at 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
      return;
    }

    if (verbose) {
      Serial.println("    Adafruit_MLX90614.begin() failed at this address.");
    }
  }

  // Fallback: in case address was changed, try to discover a readable MLX on any ACKing address.
  for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
    if (addr == kOledAddr || addr == kMax30102Addr) {
      continue;
    }
    bool isKnownCandidate = false;
    for (uint8_t candidate : kAddrCandidates) {
      if (candidate == addr) {
        isKnownCandidate = true;
        break;
      }
    }
    if (isKnownCandidate) {
      continue;
    }

    uint8_t probeErr = 0;
    if (!i2cProbe(addr, &probeErr)) {
      continue;
    }

    float obj = NAN;
    float amb = NAN;
    if (!mlxLooksReadableAt(addr, &obj, &amb)) {
      continue;
    }

    if (verbose) {
      Serial.print("  Fallback found MLX-like response at 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.print(addr, HEX);
      Serial.print(", Obj=");
      Serial.print(obj, 2);
      Serial.print("C, Amb=");
      Serial.print(amb, 2);
      Serial.println("C");
    }

    if (mlx90614.begin(addr, &Wire)) {
      g_mlxReady = true;
      g_mlxAddrInUse = addr;
      Serial.print("MLX90614 initialized at fallback address 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
      return;
    }
  }

  Serial.print("MLX90614 init failed (default addr 0x");
  if (kMlxDefaultAddr < 16) {
    Serial.print('0');
  }
  Serial.print(kMlxDefaultAddr, HEX);
  Serial.println(").");
  if (verbose) {
    Serial.println("Hint: if scan never shows 0x5A/0x5B/0x5C/0x5D, issue is wiring/power/module.");
  }
}

void updateTemperature() {
  const uint32_t now = millis();
  if (kEnableMlx90614 && g_mlxReady) {
    if (now - g_lastTempReadMs < kTempReadMs) {
      return;
    }
    g_lastTempReadMs = now;

    const float ambient = mlx90614.readAmbientTempC();
    const float object = mlx90614.readObjectTempC();
    if (isfinite(ambient) && isfinite(object)) {
      g_mlxReadFailStreak = 0;
      g_temperature.ambientC = ambient;
      g_temperature.objectC = object;
      g_temperature.valid = true;
      g_temperature.fromMax30102Die = false;
      g_temperature.lastUpdateMs = now;
      return;
    }

    g_temperature.valid = false;
    g_temperature.fromMax30102Die = false;
    if (g_mlxReadFailStreak < 255) {
      ++g_mlxReadFailStreak;
    }
    if (g_mlxReadFailStreak >= kMlxReadFailReinitThreshold) {
      Serial.println("MLX read unstable, reinit...");
      initMlx90614(false);
      g_mlxReadFailStreak = 0;
    }
    return;
  }

  if (kEnableMlx90614 && !g_mlxReady) {
    if (now - g_lastMlxRetryMs >= kMlxRetryMs) {
      g_lastMlxRetryMs = now;
      Serial.println("Retry MLX90614 init...");
      Wire.setClock(50000);
      initMlx90614(false);
      Wire.setClock(100000);
    }
  }

  if (!kEnableMax30102DieTemp) {
    g_temperature.valid = false;
    g_temperature.fromMax30102Die = false;
    return;
  }

  if (now - g_lastTempReadMs < kMax30102TempReadMs) {
    return;
  }
  g_lastTempReadMs = now;

  const float dieTemp = max30102.readTemperature();
  if (isfinite(dieTemp) && dieTemp > -40.0f && dieTemp < 85.0f) {
    g_temperature.ambientC = dieTemp;
    g_temperature.objectC = dieTemp;
    g_temperature.valid = true;
    g_temperature.fromMax30102Die = true;
    g_temperature.lastUpdateMs = now;
  } else {
    g_temperature.valid = false;
    g_temperature.fromMax30102Die = false;
    ++g_i2cErrorCount;
  }
}

void readMpuSample() {
  g_mpuSample.valid = false;
  if (!g_mpuReady) {
    return;
  }

  uint8_t buf[14] = {0};
  if (!mpuReadBytes(g_mpuAddr, kMpuAccelStartReg, buf, sizeof(buf))) {
    if (g_mpuReadFailStreak < 255) {
      ++g_mpuReadFailStreak;
    }
    if (g_mpuReadFailStreak >= kMpuReadFailReinitThreshold) {
      Serial.println("MPU read fail streak, reinit...");
      initMpu6050();
      g_mpuReadFailStreak = 0;
    }
    return;
  }
  g_mpuReadFailStreak = 0;

  const int16_t axRaw = mpuReadInt16(buf, 0);
  const int16_t ayRaw = mpuReadInt16(buf, 2);
  const int16_t azRaw = mpuReadInt16(buf, 4);
  const int16_t gxRaw = mpuReadInt16(buf, 8);
  const int16_t gyRaw = mpuReadInt16(buf, 10);
  const int16_t gzRaw = mpuReadInt16(buf, 12);

  g_mpuSample.axG = static_cast<float>(axRaw) / 16384.0f;
  g_mpuSample.ayG = static_cast<float>(ayRaw) / 16384.0f;
  g_mpuSample.azG = static_cast<float>(azRaw) / 16384.0f;
  g_mpuSample.gxDps = static_cast<float>(gxRaw) / 131.0f;
  g_mpuSample.gyDps = static_cast<float>(gyRaw) / 131.0f;
  g_mpuSample.gzDps = static_cast<float>(gzRaw) / 131.0f;
  g_mpuSample.accelMagG =
      sqrtf(g_mpuSample.axG * g_mpuSample.axG + g_mpuSample.ayG * g_mpuSample.ayG +
            g_mpuSample.azG * g_mpuSample.azG);
  g_mpuSample.gyroMaxDps =
      std::max(std::max(fabsf(g_mpuSample.gxDps), fabsf(g_mpuSample.gyDps)),
               fabsf(g_mpuSample.gzDps));
  g_mpuSample.valid = true;
}

void updateFallState(uint32_t now) {
  if (!g_mpuSample.valid) {
    return;
  }
  const FallState prevState = g_fallState;
  const float ffThr = activeFreeFallThresholdG();
  const float impactThr = activeImpactThresholdG();
  const float impactRotThr = activeImpactRotationThresholdDps();
  const uint32_t impactWinMs = activeImpactWindowMs();
  const uint32_t stillWinMs = activeStillWindowMs();

  const bool stillNow = g_mpuSample.accelMagG >= kStillAccelMinG &&
                        g_mpuSample.accelMagG <= kStillAccelMaxG &&
                        g_mpuSample.gyroMaxDps <= kStillGyroThresholdDps;
  const bool recoveredNow = g_mpuSample.accelMagG >= kRecoveryAccelThresholdG ||
                            g_mpuSample.gyroMaxDps >= kRecoveryGyroThresholdDps;

  switch (g_fallState) {
    case FallState::Normal:
      if (now >= g_cooldownUntilMs && g_mpuSample.accelMagG < ffThr) {
        g_fallState = FallState::FreeFall;
        g_freeFallMs = now;
        g_impactCandidateMs = 0;
      }
      break;

    case FallState::FreeFall:
      if (now - g_freeFallMs > impactWinMs) {
        g_fallState = FallState::Normal;
      } else if (g_mpuSample.accelMagG > impactThr && g_mpuSample.gyroMaxDps > impactRotThr) {
        if (g_impactCandidateMs == 0) {
          g_impactCandidateMs = now;
        }
        if ((now - g_impactCandidateMs) >= kImpactConfirmMinMs &&
            (now - g_freeFallMs) >= kAggressiveMotionGuardMs) {
          g_fallState = FallState::Impact;
          g_impactMs = now;
          g_stillSinceMs = 0;
        }
      } else if (g_mpuSample.accelMagG > 0.90f) {
        g_fallState = FallState::Normal;
      } else {
        g_impactCandidateMs = 0;
      }
      break;

    case FallState::Impact:
      if (recoveredNow) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kFallCooldownMs;
      } else if (stillNow) {
        if (g_stillSinceMs == 0) {
          g_stillSinceMs = now;
        }
        if ((now - g_impactMs) > stillWinMs && (now - g_stillSinceMs) > kStillConfirmMinMs) {
          g_fallState = FallState::Alert;
          g_alertStartMs = now;
          g_nextAlarmMs = now;
        }
      } else if (now - g_impactMs > 3000U) {
        g_fallState = FallState::Normal;
      } else {
        g_stillSinceMs = 0;
      }
      break;

    case FallState::Alert:
      if (recoveredNow) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kFallCooldownMs;
      } else if (now - g_alertStartMs >= kAlarmContinuousMs) {
        g_fallState = FallState::Monitor;
        g_nextAlarmMs = now;
      }
      break;

    case FallState::Monitor:
      if (recoveredNow) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kFallCooldownMs;
      } else if (now - g_alertStartMs >= (kAlarmContinuousMs + kAlarmMonitorMs)) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kFallCooldownMs;
      }
      break;
  }

  if (g_fallState != prevState) {
    if (g_fallState == FallState::FreeFall) {
      publishEvent(kEvtFallFreeFall);
    } else if (g_fallState == FallState::Impact) {
      publishEvent(kEvtFallImpact);
    } else if (g_fallState == FallState::Alert) {
      publishEvent(kEvtFallAlert);
    } else if (g_fallState == FallState::Normal &&
               (prevState == FallState::Alert || prevState == FallState::Monitor ||
                prevState == FallState::Impact || prevState == FallState::FreeFall)) {
      publishEvent(kEvtFallCleared);
    }
  }
}

void updateAlarm(uint32_t now) {
  if (!g_i2sReady) {
    return;
  }

  const bool alarmActive = (g_fallState == FallState::Alert || g_fallState == FallState::Monitor);
  if (!alarmActive) {
    if (g_pendingPromptTone) {
      ensureSpeakerActive();
      playPromptTone();
      g_pendingPromptTone = false;
    } else {
      ensureSpeakerMuted();
    }
    return;
  }

  // Fall alarm has highest priority and preempts any lower-priority prompt.
  g_pendingPromptTone = false;
  ensureSpeakerActive();

  if (g_fallState == FallState::Alert && now >= g_nextAlarmMs) {
    playAlarmBurst(false);
    g_nextAlarmMs = now + ((g_fallProfile == FallProfile::Test) ? kAlertBurstIntervalMsTest
                                                                 : kAlertBurstIntervalMs);
    return;
  }

  if (g_fallState == FallState::Monitor && now >= g_nextAlarmMs) {
    playAlarmBurst(true);
    g_nextAlarmMs = now + ((g_fallProfile == FallProfile::Test) ? kMonitorBurstIntervalMsTest
                                                                 : kMonitorBurstIntervalMs);
  }
}

void updateSensor() {
  bool consumedAnySample = false;
  max30102.check();

  while (max30102.available()) {
    consumedAnySample = true;
    const uint32_t now = millis();
    const uint32_t ir = max30102.getFIFOIR();
    const uint32_t red = max30102.getFIFORed();
    max30102.nextSample();
    g_lastMaxSampleMs = now;

    g_vitals.irValue = ir;
    g_vitals.redValue = red;
    const bool fingerNow = ir > kFingerDetectThreshold;
    if (fingerNow) {
      g_lastFingerSeenMs = now;
    }
    g_vitals.fingerDetected = fingerNow || ((now - g_lastFingerSeenMs) <= kFingerLostDebounceMs);
    if (g_vitals.fingerDetected != g_lastFingerEventState) {
      g_lastFingerEventState = g_vitals.fingerDetected;
      publishEvent(g_vitals.fingerDetected ? kEvtFingerDetected : kEvtNoFinger);
    }

    if (!g_vitals.fingerDetected) {
      g_sensorState = SensorState::WaitingFinger;
      g_calibrationPiSum = 0.0f;
      g_calibrationWindowCount = 0;
      g_guidanceHint = GuidanceHint::NoFinger;
      resetMeasurementFilters();
      continue;
    }

    if (g_sensorState == SensorState::WaitingFinger) {
      startCalibration(now);
    }

    if (g_bufferCount < kAlgoWindowSize) {
      g_irBuffer[g_bufferCount] = ir;
      g_redBuffer[g_bufferCount] = red;
      ++g_bufferCount;
    } else {
      memmove(g_irBuffer, g_irBuffer + 1, (kAlgoWindowSize - 1) * sizeof(uint32_t));
      memmove(g_redBuffer, g_redBuffer + 1, (kAlgoWindowSize - 1) * sizeof(uint32_t));
      g_irBuffer[kAlgoWindowSize - 1] = ir;
      g_redBuffer[kAlgoWindowSize - 1] = red;
    }
    ++g_newSamplesSinceCalc;

    if (g_bufferCount == kAlgoWindowSize && g_newSamplesSinceCalc >= kAlgoStepSamples) {
      g_newSamplesSinceCalc = 0;

      int32_t spo2 = 0;
      int8_t spo2Valid = 0;
      int32_t algoHeartRate = 0;
      int8_t algoHeartRateValid = 0;

      maxim_heart_rate_and_oxygen_saturation(
          g_irBuffer, static_cast<int32_t>(kAlgoWindowSize), g_redBuffer, &spo2, &spo2Valid,
          &algoHeartRate, &algoHeartRateValid);

      uint32_t irMin = g_irBuffer[0];
      uint32_t irMax = g_irBuffer[0];
      uint32_t redMin = g_redBuffer[0];
      uint32_t redMax = g_redBuffer[0];
      uint64_t irSum = 0;
      uint64_t redSum = 0;
      for (size_t i = 0; i < kAlgoWindowSize; ++i) {
        irMin = std::min(irMin, g_irBuffer[i]);
        irMax = std::max(irMax, g_irBuffer[i]);
        redMin = std::min(redMin, g_redBuffer[i]);
        redMax = std::max(redMax, g_redBuffer[i]);
        irSum += g_irBuffer[i];
        redSum += g_redBuffer[i];
      }

      const uint32_t irDcU32 = static_cast<uint32_t>(irSum / static_cast<uint64_t>(kAlgoWindowSize));
      const float irDc = static_cast<float>(irDcU32);
      const float irAcP2P = static_cast<float>(irMax - irMin);
      const uint32_t redDcU32 =
          static_cast<uint32_t>(redSum / static_cast<uint64_t>(kAlgoWindowSize));
      const float redAcP2P = static_cast<float>(redMax - redMin);
      const float perfusionIndex = (irDc > 0.0f) ? (irAcP2P / irDc) : 0.0f;
      g_currentPerfusionIndex = perfusionIndex;
      const float qualityRatio =
          perfusionIndex / std::max(g_calibratedPerfusionIndex, kMinPerfusionIndex);
      const bool signalQualityOk = perfusionIndex >= kMinPerfusionIndex;
      g_signalConfidence = calcSignalConfidence(qualityRatio, g_vitals.heartRateValid, g_vitals.spo2Valid);
      g_guidanceHint =
          evaluateGuidance(qualityRatio, perfusionIndex, g_vitals.fingerDetected, g_mpuSample.accelMagG, 0.0f);
      autoTuneLedAmplitude(irDcU32, now);

      if (g_sensorState == SensorState::Calibrating) {
        if (signalQualityOk) {
          g_calibrationPiSum += perfusionIndex;
          if (g_calibrationWindowCount < 255) {
            ++g_calibrationWindowCount;
          }
        }

        if ((now - g_calibrationStartMs) >= kCalibrationDurationMs &&
            g_calibrationWindowCount >= kCalibrationMinWindows) {
          g_calibratedPerfusionIndex =
              std::max(g_calibrationPiSum / g_calibrationWindowCount, kMinPerfusionIndex);
          g_sensorState = SensorState::Running;
        }
        continue;
      }

      const bool algoHrAccepted = signalQualityOk && (qualityRatio >= kAlgoHrMinQualityRatio) &&
                                  (g_signalConfidence >= 30) &&
                                  algoHeartRateValid &&
                                  algoHeartRate >= static_cast<int32_t>(kMinAcceptedBpm) &&
                                  algoHeartRate <= static_cast<int32_t>(kMaxAcceptedBpm);
      if (algoHrAccepted) {
        const float algoBpm = static_cast<float>(algoHeartRate);
        bool jumpOk = true;
        if (g_vitals.heartRateBpm > 0.0f) {
          jumpOk = fabsf(algoBpm - g_vitals.heartRateBpm) <= (kMaxHrJumpPerUpdate + 12.0f);
        }
        if (jumpOk && g_vitals.heartRateDisplayValid) {
          const float displayDeltaLimit =
              (g_signalConfidence < kLowHrGuardMinConfidence) ? 10.0f : kAlgoHrMaxDeltaFromDisplay;
          jumpOk = fabsf(algoBpm - g_vitals.heartRateDisplayBpm) <= displayDeltaLimit;
        }

        if (jumpOk && algoBpm < kLowHrGuardFloorBpm) {
          if (g_signalConfidence < kLowHrGuardMinConfidence) {
            jumpOk = false;
          } else {
            if (g_lowHrCandidateStreak < 255) {
              ++g_lowHrCandidateStreak;
            }
            if (g_lowHrCandidateStreak < kLowHrGuardConsecutiveRequired &&
                g_vitals.heartRateDisplayValid) {
              jumpOk = false;
            }
          }
        } else if (algoBpm >= kLowHrGuardFloorBpm) {
          g_lowHrCandidateStreak = 0;
        }

        if (jumpOk) {
          if (!g_vitals.heartRateValid) {
            g_vitals.heartRateValid = true;
            g_vitals.heartRateBpm = algoBpm;
          } else {
            g_vitals.heartRateBpm =
                (1.0f - kHrAlgoFallbackAlpha) * g_vitals.heartRateBpm + kHrAlgoFallbackAlpha * algoBpm;
          }
          g_vitals.lastHrUpdateMs = now;

          const bool beatStale = !g_vitals.heartRateRealtimeValid ||
                                 (now - g_vitals.lastHrBeatAcceptedMs) > kBeatRealtimeStaleMs;
          if (beatStale) {
            if (!g_vitals.heartRateDisplayValid) {
              g_vitals.heartRateDisplayBpm = algoBpm;
            } else {
              g_vitals.heartRateDisplayBpm =
                  (1.0f - kHrAlgoFallbackAlpha) * g_vitals.heartRateDisplayBpm +
                  kHrAlgoFallbackAlpha * algoBpm;
            }
            g_vitals.heartRateDisplayValid = true;
            g_vitals.lastHrDisplayRefreshMs = now;
    }
  }

  const uint32_t now = millis();
  if (!consumedAnySample && (now - g_lastMaxSampleMs) > kMaxSampleStallMs) {
    if (g_maxSampleStallCount < 255) {
      ++g_maxSampleStallCount;
    }
    if (g_maxSampleStallCount >= kMaxSampleStallReinitThreshold) {
      Serial.println("MAX30102 sample stall, reinit...");
      initMax30102();
      g_maxSampleStallCount = 0;
      publishEvent(kEvtSensorRecovered);
    }
    g_lastMaxSampleMs = now;
  }
}

      float fallbackSpo2 = 0.0f;
      const bool fallbackSpo2Valid =
          estimateSpo2FromWindowFallback(irDcU32, irAcP2P, redDcU32, redAcP2P, &fallbackSpo2);
      const bool algoSpo2Accepted = spo2Valid && (qualityRatio >= (kSpo2MinQualityRatio * 0.5f)) &&
                                    spo2 >= kMinAcceptedSpo2 && spo2 <= kMaxAcceptedSpo2;

      bool spo2CandidateReady = false;
      bool spo2FromAlgo = false;
      float spo2Candidate = 0.0f;
      if (algoSpo2Accepted) {
        spo2Candidate = static_cast<float>(spo2);
        spo2CandidateReady = true;
        spo2FromAlgo = true;
      } else if (fallbackSpo2Valid) {
        spo2Candidate = fallbackSpo2;
        spo2CandidateReady = true;
      }

      if (spo2CandidateReady) {
        const float spo2Median = pushAndMedian(
            spo2Candidate, g_spo2History, &g_spo2HistoryIndex, &g_spo2HistoryCount);

        bool jumpOk = true;
        if (g_vitals.spo2Percent > 0.0f) {
          float limit = g_vitals.spo2Valid
                            ? (spo2FromAlgo ? kMaxSpo2JumpPerUpdate : kMaxSpo2FallbackJumpPerUpdate)
                            : kMaxSpo2ReacquireJump;
          if (g_vitals.spo2Valid && qualityRatio < 0.25f) {
            limit += 1.5f;
          }
          jumpOk = fabsf(spo2Median - g_vitals.spo2Percent) <= limit;
        }

        if (jumpOk) {
          g_spo2InvalidStreak = 0;
          if (g_spo2ValidStreak < 255) {
            ++g_spo2ValidStreak;
          }

          if (!g_vitals.spo2Valid) {
            if (g_spo2ValidStreak >= kSpo2ValidStreakRequired) {
              g_vitals.spo2Valid = true;
              g_vitals.spo2Percent = spo2Median;
              g_vitals.lastSpo2UpdateMs = now;
            }
          } else {
            g_vitals.spo2Percent =
                (1.0f - kSpo2EwmaAlpha) * g_vitals.spo2Percent + kSpo2EwmaAlpha * spo2Median;
            g_vitals.lastSpo2UpdateMs = now;
          }

          float spo2Realtime = spo2Candidate;
          if (g_vitals.spo2RealtimeValid) {
            const float delta = spo2Realtime - g_vitals.spo2RealtimePercent;
            if (fabsf(delta) > kSpo2RealtimeMaxJump) {
              spo2Realtime = g_vitals.spo2RealtimePercent +
                             ((delta > 0.0f) ? kSpo2RealtimeMaxJump : -kSpo2RealtimeMaxJump);
            }
          }
          g_vitals.spo2RealtimePercent = spo2Realtime;
          g_vitals.spo2RealtimeValid = true;

          if (!g_vitals.spo2DisplayValid) {
            g_vitals.spo2DisplayPercent = spo2Realtime;
          } else {
            g_vitals.spo2DisplayPercent =
                (1.0f - kSpo2DisplayAlpha) * g_vitals.spo2DisplayPercent +
                kSpo2DisplayAlpha * spo2Realtime;
          }
          g_vitals.spo2DisplayValid = true;
          g_vitals.lastSpo2DisplayRefreshMs = now;
        } else {
          g_spo2ValidStreak = 0;
          if (g_spo2InvalidStreak < 255) {
            g_spo2InvalidStreak += (qualityRatio < 0.3f) ? 1 : 2;
            if (g_spo2InvalidStreak > 255) {
              g_spo2InvalidStreak = 255;
            }
          }
          if (g_vitals.spo2DisplayValid && qualityRatio < 0.30f) {
            g_vitals.spo2DisplayPercent =
                std::max(82.0f, g_vitals.spo2DisplayPercent - 0.12f);
            g_vitals.lastSpo2DisplayRefreshMs = now;
          }
        }
      } else {
        g_spo2ValidStreak = 0;
        if (g_spo2InvalidStreak < 255) {
          g_spo2InvalidStreak += (qualityRatio < 0.3f) ? 1 : 2;
          if (g_spo2InvalidStreak > 255) {
            g_spo2InvalidStreak = 255;
          }
        }
        if (g_vitals.spo2DisplayValid && qualityRatio < 0.30f) {
          g_vitals.spo2DisplayPercent =
              std::max(82.0f, g_vitals.spo2DisplayPercent - 0.14f);
          g_vitals.lastSpo2DisplayRefreshMs = now;
        }
      }

      if (g_guidanceHint == GuidanceHint::None) {
        g_guidanceHint = evaluateGuidance(qualityRatio, perfusionIndex, g_vitals.fingerDetected,
                                          g_mpuSample.accelMagG, spo2CandidateReady ? spo2Candidate : 0.0f);
      }

      if (g_spo2InvalidStreak >= kInvalidStreakDrop &&
          (now - g_vitals.lastSpo2UpdateMs) > kSpo2DisplayHoldMs) {
        g_vitals.spo2Valid = false;
        g_vitals.spo2Percent = 0.0f;
        g_vitals.spo2DisplayValid = false;
        g_vitals.spo2RealtimeValid = false;
        g_vitals.spo2DisplayPercent = 0.0f;
        g_vitals.spo2RealtimePercent = 0.0f;
      }
    }

    if (g_sensorState == SensorState::Running && checkForBeat(ir)) {
      if (g_lastBeatMs > 0) {
        const uint32_t delta = now - g_lastBeatMs;
        if (delta > 0) {
          const float bpm = 60000.0f / static_cast<float>(delta);
          bool accepted = bpm >= kMinAcceptedBpm && bpm <= kMaxAcceptedBpm;
          if (accepted && g_vitals.heartRateValid &&
              fabsf(bpm - g_vitals.heartRateBpm) > kMaxHrJumpPerUpdate) {
            accepted = false;
          }
          if (accepted && g_vitals.heartRateDisplayValid) {
            const float displayDeltaLimit =
                (g_signalConfidence < kLowHrGuardMinConfidence) ? 10.0f : kMaxHrJumpPerUpdate;
            if (fabsf(bpm - g_vitals.heartRateDisplayBpm) > displayDeltaLimit) {
              accepted = false;
            }
          }
          if (accepted && bpm < kLowHrGuardFloorBpm) {
            if (g_signalConfidence < kLowHrGuardMinConfidence) {
              accepted = false;
            } else {
              if (g_lowHrCandidateStreak < 255) {
                ++g_lowHrCandidateStreak;
              }
              if (g_lowHrCandidateStreak < kLowHrGuardConsecutiveRequired &&
                  g_vitals.heartRateDisplayValid) {
                accepted = false;
              }
            }
          } else if (bpm >= kLowHrGuardFloorBpm) {
            g_lowHrCandidateStreak = 0;
          }

          if (accepted) {
            const float hrMedian =
                pushAndMedian(bpm, g_hrHistory, &g_hrHistoryIndex, &g_hrHistoryCount);
            g_hrInvalidStreak = 0;
            if (g_hrValidStreak < 255) {
              ++g_hrValidStreak;
            }

            if (!g_vitals.heartRateValid) {
              if (g_hrValidStreak >= kHrValidStreakRequired) {
                g_vitals.heartRateValid = true;
                g_vitals.heartRateBpm = hrMedian;
                g_vitals.lastHrUpdateMs = now;
              }
            } else {
              g_vitals.heartRateBpm =
                  (1.0f - kHrEwmaAlpha) * g_vitals.heartRateBpm + kHrEwmaAlpha * hrMedian;
              g_vitals.lastHrUpdateMs = now;
            }

            float realtime = bpm;
            if (g_vitals.heartRateRealtimeValid) {
              const float delta = realtime - g_vitals.heartRateRealtimeBpm;
              if (fabsf(delta) > kHrRealtimeMaxJump) {
                realtime = g_vitals.heartRateRealtimeBpm +
                           ((delta > 0.0f) ? kHrRealtimeMaxJump : -kHrRealtimeMaxJump);
              }
            }
            g_vitals.heartRateRealtimeBpm = realtime;
            g_vitals.heartRateRealtimeValid = true;

            if (!g_vitals.heartRateDisplayValid) {
              g_vitals.heartRateDisplayBpm = realtime;
            } else {
            g_vitals.heartRateDisplayBpm = (1.0f - kHrDisplayAlpha) * g_vitals.heartRateDisplayBpm +
                                             kHrDisplayAlpha * realtime;
            }
            g_vitals.heartRateDisplayValid = true;
            g_vitals.lastHrBeatAcceptedMs = now;
            g_vitals.lastHrDisplayRefreshMs = now;
          } else {
            g_hrValidStreak = 0;
            if (g_hrInvalidStreak < 255) {
              ++g_hrInvalidStreak;
            }
          }
        }
      }
      g_lastBeatMs = now;
    }
  }

  if (g_vitals.fingerDetected && g_vitals.heartRateValid &&
      (millis() - g_vitals.lastHrUpdateMs) > kBeatTimeoutMs) {
    g_vitals.heartRateValid = false;
  }

  if (g_vitals.fingerDetected && g_vitals.heartRateDisplayValid &&
      (millis() - g_vitals.lastHrDisplayRefreshMs) > kHrDisplayHoldMs) {
    g_vitals.heartRateDisplayValid = false;
    g_vitals.heartRateRealtimeValid = false;
    g_vitals.heartRateDisplayBpm = 0.0f;
    g_vitals.heartRateRealtimeBpm = 0.0f;
  }

  if (g_vitals.fingerDetected && g_vitals.spo2Valid &&
      (millis() - g_vitals.lastSpo2UpdateMs) > kBeatTimeoutMs) {
    g_vitals.spo2Valid = false;
    g_vitals.spo2Percent = 0.0f;
  }

  if (g_vitals.fingerDetected && g_vitals.spo2DisplayValid &&
      (millis() - g_vitals.lastSpo2DisplayRefreshMs) > kSpo2DisplayHoldMs) {
    g_vitals.spo2DisplayValid = false;
    g_vitals.spo2RealtimeValid = false;
    g_vitals.spo2DisplayPercent = 0.0f;
    g_vitals.spo2RealtimePercent = 0.0f;
  }
}

void renderUi() {
  const uint32_t now = millis();
  if (now - g_lastUiRefreshMs < kUiRefreshMs) {
    return;
  }
  g_lastUiRefreshMs = now;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("HR");
  display.setCursor(70, 0);
  display.print("O2");

  display.setTextSize(2);
  display.setCursor(0, 10);
  if (g_vitals.heartRateDisplayValid) {
    display.print(static_cast<int>(lroundf(g_vitals.heartRateDisplayBpm)));
  } else {
    display.print("--");
  }

  display.setCursor(70, 10);
  if (g_vitals.spo2DisplayValid) {
    display.print(static_cast<int>(lroundf(g_vitals.spo2DisplayPercent)));
    display.setTextSize(1);
    display.print("%");
    display.setTextSize(2);
  } else {
    display.print("--");
  }

  display.drawLine(0, 31, 127, 31, SSD1306_WHITE);
  display.drawLine(63, 32, 63, 63, SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(0, 34);
  display.print("TMP");
  display.setCursor(72, 34);
  display.print("FALL");

  display.setTextSize(2);
  display.setCursor(0, 44);
  if (g_temperature.valid) {
    char tempBuf[8] = {0};
    snprintf(tempBuf, sizeof(tempBuf), "%.1f", g_temperature.objectC);
    display.print(tempBuf);
  } else {
    display.print("--.-");
  }

  display.setCursor(72, 44);
  display.print(fallStateUiText(g_fallState));
  display.setTextSize(1);
  display.setCursor(72, 58);
  display.print(guidanceUiText(g_guidanceHint));

  display.display();

  if (now - g_lastSerialLogMs < kSerialLogMs) {
    return;
  }
  g_lastSerialLogMs = now;

  Serial.print("HR=");
  if (g_vitals.heartRateDisplayValid) {
    Serial.print(g_vitals.heartRateDisplayBpm, 1);
  } else {
    Serial.print("--");
  }
  Serial.print(" O2=");
  if (g_vitals.spo2DisplayValid) {
    Serial.print(g_vitals.spo2DisplayPercent, 1);
    Serial.print("%");
  } else {
    Serial.print("--%");
  }
  Serial.print(" HRr=");
  if (g_vitals.heartRateRealtimeValid) {
    Serial.print(g_vitals.heartRateRealtimeBpm, 1);
  } else {
    Serial.print("--");
  }
  Serial.print(" O2r=");
  if (g_vitals.spo2RealtimeValid) {
    Serial.print(g_vitals.spo2RealtimePercent, 1);
    Serial.print("%");
  } else {
    Serial.print("--%");
  }
  Serial.print(" T=");
  if (g_temperature.valid) {
    Serial.print(g_temperature.objectC, 1);
  } else {
    Serial.print("--");
  }
  Serial.print(" F=");
  Serial.print(fallStateText(g_fallState));
  Serial.print(" Thr(FF<");
  Serial.print(activeFreeFallThresholdG(), 2);
  Serial.print(" IMP>");
  Serial.print(activeImpactThresholdG(), 2);
  Serial.print(" G>");
  Serial.print(activeImpactRotationThresholdDps(), 0);
  Serial.print(")");
  Serial.print(" Q=");
  Serial.print(qualityText(currentSignalQuality()));
  Serial.print(" C=");
  Serial.print(g_signalConfidence);
  Serial.print(" H=");
  Serial.print(guidanceText(g_guidanceHint));
  Serial.print(" S=");
  Serial.print(sensorStateText());
  Serial.print(" LED=0x");
  if (g_ledAmplitude < 16) {
    Serial.print('0');
  }
  Serial.print(g_ledAmplitude, HEX);
  Serial.print(" M=");
  Serial.print(systemModeText(g_systemMode));
  Serial.print("/");
  Serial.print(fallProfileText(g_fallProfile));
  if (kEnableMicVad) {
    Serial.print(" V=");
    if (!g_voiceCalibrated) {
      Serial.print("CAL");
    } else {
      Serial.print(g_voiceActive ? "ON" : "OFF");
    }
    Serial.print(" VRMS=");
    Serial.print(g_voiceRms, 3);
    Serial.print(" VS=");
    Serial.print(voiceStateText(g_voiceState));
  }
  Serial.print(" E=");
  Serial.print(g_i2cErrorCount);
  Serial.print("/");
  Serial.print(g_mpuReadFailStreak);
  Serial.print("/");
  Serial.print(g_mlxReadFailStreak);
  Serial.print("/");
  Serial.print(g_micReadFailStreak);
  Serial.print(" Src=");
  if (g_temperature.valid) {
    Serial.print(g_temperature.fromMax30102Die ? "MAX" : "MLX");
  } else {
    Serial.print("NONE");
  }
  Serial.println();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Booting ESP32 AI Health Assistant...");
  g_fallProfile = FallProfile::Normal;
  printFallThresholds();
  Serial.println("Cmd: MODE TEST | MODE NORMAL | MODE? | HEALTH?");

  initI2c();
  i2cScan();
  initOled();
  initMax30102();
  initMpu6050();
  initI2sAudio();
  initMicI2s();
  if (kEnableMlx90614) {
    initMlx90614(true);
  } else {
    Serial.println("MLX90614 disabled, using MAX30102 die temperature.");
  }

  g_systemMode = SystemMode::Monitoring;
  g_sensorFaultLatched = (!g_mpuReady || !g_i2sReady || (kEnableMicVad && !g_micReady));
  if (g_sensorFaultLatched) {
    publishEvent(kEvtSensorError);
  } else {
    publishEvent(kEvtSensorRecovered);
  }
}

void loop() {
  const uint32_t loopStartUs = micros();
  const uint32_t now = millis();
  pollSerialCommands(now);
  updateSensor();
  if (now - g_lastMpuReadMs >= kMpuReadMs) {
    g_lastMpuReadMs = now;
    readMpuSample();
  }
  updateFallState(now);
  updateAlarm(now);
  updateVoiceVad(now);
  updateTemperature();
  updateSystemModeAndEvents(now);
  renderUi();
  updateHealthSnapshot(now, loopStartUs);
}
