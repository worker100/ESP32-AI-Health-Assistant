#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace features {
namespace telemetry {

struct ContextLogView {
  bool heartRateValid = false;
  float heartRateBpm = 0.0f;
  bool spo2Valid = false;
  float spo2Percent = 0.0f;
  bool temperatureValid = false;
  float temperatureC = 0.0f;
  bool fingerDetected = false;
  const char* quality = "NO";
  const char* fall = "NORMAL";
  const char* hint = "NONE";
  const char* confidence = "invalid";
  const char* temperatureValidity = "unavailable";
  const char* temperatureSource = "NONE";
};

struct HealthLineView {
  bool heartRateDisplayValid = false;
  float heartRateDisplayBpm = 0.0f;
  bool spo2DisplayValid = false;
  float spo2DisplayPercent = 0.0f;
  bool heartRateRealtimeValid = false;
  float heartRateRealtimeBpm = 0.0f;
  bool spo2RealtimeValid = false;
  float spo2RealtimePercent = 0.0f;
  bool temperatureValid = false;
  float temperatureC = 0.0f;
  float freeFallThresholdG = 0.0f;
  float impactThresholdG = 0.0f;
  float impactRotationThresholdDps = 0.0f;
  uint8_t signalConfidence = 0;
  uint8_t ledAmplitude = 0;
  float voiceRms = 0.0f;
  bool voiceEnabled = false;
  bool voiceCalibrated = false;
  bool voiceActive = false;
  uint32_t i2cErr = 0;
  uint8_t mpuFail = 0;
  uint8_t mlxFail = 0;
  uint8_t micFail = 0;
  const char* fallState = "NORMAL";
  const char* quality = "NO";
  const char* hint = "NONE";
  const char* sensorState = "WAIT";
  const char* systemMode = "BOOT";
  const char* fallProfile = "NORMAL";
  const char* voiceState = "IDLE";
  const char* temperatureSource = "NONE";
};

void printContextLine(const ContextLogView& view);
void printHealthLine(const HealthLineView& view);

}  // namespace telemetry
}  // namespace features

