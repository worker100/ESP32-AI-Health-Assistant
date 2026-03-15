#pragma once

#include <stddef.h>
#include <stdint.h>
#include <driver/i2s.h>
#include "project_config.h"

static constexpr uint8_t kI2cSda = 8;
static constexpr uint8_t kI2cScl = 9;
static constexpr uint8_t kOledAddr = 0x3C;
static constexpr uint8_t kMax30102Addr = 0x57;
static constexpr uint8_t kMlxDefaultAddr = 0x5A;
static constexpr bool kEnableMlx90614 = true;
static constexpr bool kEnableMax30102DieTemp = true;
static constexpr uint32_t kMax30102TempReadMs = 2000;
static constexpr int kScreenWidth = 128;
static constexpr int kScreenHeight = 64;
#ifndef AI_HEALTH_UI_REFRESH_MS
#define AI_HEALTH_UI_REFRESH_MS 250
#endif
#ifndef AI_HEALTH_TEMP_READ_MS
#define AI_HEALTH_TEMP_READ_MS 1000
#endif
static constexpr uint32_t kUiRefreshMs = AI_HEALTH_UI_REFRESH_MS;
static constexpr uint32_t kSerialLogMs = 1000;
static constexpr uint32_t kTempReadMs = AI_HEALTH_TEMP_READ_MS;
static constexpr uint32_t kMlxRetryMs = 5000;
static constexpr uint32_t kBeatTimeoutMs = 12000;
static constexpr uint32_t kCalibrationDurationMs = 3000;
static constexpr uint32_t kFingerLostDebounceMs = 1800;
static constexpr float kHrDisplayAlpha = 0.35f;
static constexpr float kHrRealtimeMaxJump = 10.0f;
static constexpr float kSpo2DisplayAlpha = 0.32f;
static constexpr float kSpo2RealtimeMaxJump = 2.5f;
static constexpr size_t kAlgoWindowSize = 100;
static constexpr size_t kAlgoStepSamples = 25;
static constexpr float kHrEwmaAlpha = 0.30f;
static constexpr float kHrAlgoFallbackAlpha = 0.22f;
static constexpr float kSpo2EwmaAlpha = 0.25f;
static constexpr uint8_t kMedianWindowSize = 5;
static constexpr float kMaxHrJumpPerUpdate = 16.0f;
static constexpr float kMaxSpo2JumpPerUpdate = 5.5f;
static constexpr float kMinPerfusionIndex = 0.0012f;
static constexpr uint8_t kHrValidStreakRequired = 2;
static constexpr uint8_t kSpo2ValidStreakRequired = 1;
static constexpr uint8_t kInvalidStreakDrop = 28;
static constexpr float kMaxSpo2ReacquireJump = 7.0f;
static constexpr float kSpo2MinQualityRatio = 0.08f;
static constexpr float kMaxSpo2FallbackJumpPerUpdate = 4.8f;
static constexpr uint32_t kSpo2MinIrDc = 12000;
static constexpr uint32_t kSpo2MinRedDc = 7000;
static constexpr float kSpo2MinAcP2P = 60.0f;
static constexpr uint8_t kCalibrationMinWindows = 1;
static constexpr uint32_t kBeatRealtimeStaleMs = 2500;
static constexpr float kAlgoHrMinQualityRatio = 0.28f;
static constexpr float kAlgoHrMaxDeltaFromDisplay = 15.0f;
static constexpr uint32_t kAlgoHrFreshMs = 4000;
static constexpr float kHrBeatAlgoAgreeLowMotionBpm = 14.0f;
static constexpr float kHrBeatAlgoAgreeHighMotionBpm = 10.0f;
static constexpr float kHrMotionAccelDevQuiet = 0.10f;
static constexpr float kHrMotionGyroQuietDps = 45.0f;
static constexpr float kHrMotionAccelDevHard = 0.22f;
static constexpr float kHrMotionGyroHardDps = 85.0f;
static constexpr uint8_t kSpo2FallbackMaxConsecutive = 3;
static constexpr float kSpo2FallbackMinQualityRatio = 0.30f;
static constexpr uint8_t kSpo2FallbackMinConfidence = 45;
static constexpr float kLowHrGuardFloorBpm = 52.0f;
static constexpr uint8_t kLowHrGuardMinConfidence = 40;
static constexpr uint8_t kLowHrGuardConsecutiveRequired = 3;
static constexpr long kFingerDetectThreshold = 50000;
static constexpr uint32_t kIrTargetLow = 75000;
static constexpr uint32_t kIrTargetHigh = 150000;
static constexpr uint8_t kLedAmplitudeMin = 0x08;
static constexpr uint8_t kLedAmplitudeMax = 0x5F;
static constexpr uint8_t kLedAmplitudeStep = 2;
static constexpr uint32_t kLedTuneIntervalMs = 1000;
static constexpr uint8_t kMpuAddr0 = 0x68;
static constexpr uint8_t kMpuAddr1 = 0x69;
static constexpr uint8_t kMpuWhoAmIReg = 0x75;
static constexpr uint8_t kMpuPwrMgmt1Reg = 0x6B;
static constexpr uint8_t kMpuAccelConfigReg = 0x1C;
static constexpr uint8_t kMpuGyroConfigReg = 0x1B;
static constexpr uint8_t kMpuAccelStartReg = 0x3B;
static constexpr uint32_t kMpuReadMs = 100;
static constexpr uint32_t kAlarmContinuousMs = 12000;
static constexpr uint32_t kAlarmMonitorMs = 30000;
static constexpr uint32_t kImpactWindowMsNormal = 1300;
static constexpr uint32_t kStillWindowMsNormal = 700;
static constexpr uint32_t kImpactWindowMsTest = 900;
static constexpr uint32_t kStillWindowMsTest = 550;
static constexpr uint32_t kFallCooldownMs = 3000;
static constexpr uint32_t kAlertBurstIntervalMs = 900;
static constexpr uint32_t kMonitorBurstIntervalMs = 3000;
static constexpr uint32_t kAlertBurstIntervalMsTest = 700;
static constexpr uint32_t kMonitorBurstIntervalMsTest = 2200;
static constexpr float kAlarmToneHighHz = 1320.0f;
static constexpr float kAlarmToneMidHz = 900.0f;
static constexpr float kPromptToneHz = 660.0f;
static constexpr float kFreeFallThresholdGNormal = 0.80f;
static constexpr float kImpactThresholdGNormal = 0.95f;
static constexpr float kImpactRotationThresholdDpsNormal = 20.0f;
static constexpr float kFreeFallThresholdGTest = 0.72f;
static constexpr float kImpactThresholdGTest = 0.90f;
static constexpr float kImpactRotationThresholdDpsTest = 16.0f;
static constexpr float kStillAccelMinG = 0.85f;
static constexpr float kStillAccelMaxG = 1.15f;
static constexpr float kStillGyroThresholdDps = 40.0f;
static constexpr float kRecoveryAccelThresholdG = 1.55f;
static constexpr float kRecoveryGyroThresholdDps = 110.0f;
static constexpr uint32_t kImpactConfirmMinMs = 80;
static constexpr uint32_t kStillConfirmMinMs = 180;
// Avoid clearing Impact too early due immediate post-impact rebound.
static constexpr uint32_t kImpactRecoveryGuardMs = 260;
static constexpr uint32_t kRecoveryConfirmMinMs = 220;
static constexpr uint32_t kAggressiveMotionGuardMs = 220;
static constexpr i2s_port_t kI2sPort = I2S_NUM_0;
static constexpr int kI2sSampleRate = 16000;
static constexpr int kI2sBclkPin = 4;
static constexpr int kI2sLrcPin = 5;
static constexpr int kI2sDinPin = 6;
static constexpr int kI2sBitsPerSample = 16;
static constexpr size_t kI2sAudioBufferSamples = 128;
static constexpr bool kEnableMicVad = true;
static constexpr i2s_port_t kMicI2sPort = I2S_NUM_1;
static constexpr int kMicSampleRate = 16000;
static constexpr int kMicBclkPin = 15;
static constexpr int kMicWsPin = 16;
static constexpr int kMicDinPin = 17;
static constexpr size_t kMicVadSamples = 256;
static constexpr uint32_t kMicVadUpdateMs = 120;
static constexpr uint32_t kSerialCmdPollMs = 120;
static constexpr uint32_t kHealthLogMs = 5000;
static constexpr uint8_t kMpuReadFailReinitThreshold = 6;
static constexpr uint8_t kMicReadFailReinitThreshold = 10;
static constexpr uint8_t kMlxReadFailReinitThreshold = 6;
static constexpr uint32_t kMaxSampleStallMs = 4000;
static constexpr uint8_t kMaxSampleStallReinitThreshold = 2;
static constexpr size_t kBackendMicFrameSamples = 320;  // 20ms at 16kHz
static constexpr size_t kBackendMicChunkBytes = kBackendMicFrameSamples * sizeof(int16_t);
static constexpr size_t kBackendMicBase64Cap = 1024;
static constexpr size_t kBackendTtsChunkBytes = 1024;

static constexpr uint32_t kEvtFallFreeFall = 1UL << 0;
static constexpr uint32_t kEvtFallImpact = 1UL << 1;
static constexpr uint32_t kEvtFallAlert = 1UL << 2;
static constexpr uint32_t kEvtFallCleared = 1UL << 3;
static constexpr uint32_t kEvtNoFinger = 1UL << 4;
static constexpr uint32_t kEvtFingerDetected = 1UL << 5;
static constexpr uint32_t kEvtSensorError = 1UL << 6;
static constexpr uint32_t kEvtSensorRecovered = 1UL << 7;
static constexpr uint32_t kEvtVoiceWakeReserved = 1UL << 8;
static constexpr uint32_t kEvtVoiceListenStart = 1UL << 9;
static constexpr uint32_t kEvtVoiceListenEnd = 1UL << 10;
static constexpr uint32_t kEvtFallProfileChanged = 1UL << 11;

