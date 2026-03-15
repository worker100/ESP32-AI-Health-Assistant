#pragma once

#include <stdint.h>

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

enum class LogProfile {
  Demo,
  Full,
};

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

struct AiHealthContextSnapshot {
  bool heartRateValid = false;
  bool spo2Valid = false;
  bool temperatureValid = false;
  float heartRateBpm = 0.0f;
  float spo2Percent = 0.0f;
  float temperatureC = 0.0f;
  bool fingerDetected = false;
  SignalQuality signalQuality = SignalQuality::NoFinger;
  FallState fallState = FallState::Normal;
  GuidanceHint guidanceHint = GuidanceHint::None;
  const char* measurementConfidence = "invalid";
  const char* temperatureValidity = "unavailable";
  const char* tempSource = "NONE";
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

