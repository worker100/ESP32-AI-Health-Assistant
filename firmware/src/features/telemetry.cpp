#include "telemetry.h"

namespace features {
namespace telemetry {

void printContextLine(const ContextLogView& view) {
  Serial.print("CTX hr=");
  if (view.heartRateValid) {
    Serial.print(view.heartRateBpm, 1);
  } else {
    Serial.print("null");
  }
  Serial.print(" spo2=");
  if (view.spo2Valid) {
    Serial.print(view.spo2Percent, 1);
  } else {
    Serial.print("null");
  }
  Serial.print(" temp=");
  if (view.temperatureValid) {
    Serial.print(view.temperatureC, 1);
  } else {
    Serial.print("null");
  }
  Serial.print(" finger=");
  Serial.print(view.fingerDetected ? "true" : "false");
  Serial.print(" quality=");
  Serial.print(view.quality);
  Serial.print(" fall=");
  Serial.print(view.fall);
  Serial.print(" hint=");
  Serial.print(view.hint);
  Serial.print(" conf=");
  Serial.print(view.confidence);
  Serial.print(" tempv=");
  Serial.print(view.temperatureValidity);
  Serial.print(" src=");
  Serial.println(view.temperatureSource);
}

void printHealthLine(const HealthLineView& view) {
  Serial.print("HR=");
  if (view.heartRateDisplayValid) {
    Serial.print(view.heartRateDisplayBpm, 1);
  } else {
    Serial.print("--");
  }
  Serial.print(" O2=");
  if (view.spo2DisplayValid) {
    Serial.print(view.spo2DisplayPercent, 1);
    Serial.print("%");
  } else {
    Serial.print("--%");
  }
  Serial.print(" HRr=");
  if (view.heartRateRealtimeValid) {
    Serial.print(view.heartRateRealtimeBpm, 1);
  } else {
    Serial.print("--");
  }
  Serial.print(" O2r=");
  if (view.spo2RealtimeValid) {
    Serial.print(view.spo2RealtimePercent, 1);
    Serial.print("%");
  } else {
    Serial.print("--%");
  }
  Serial.print(" T=");
  if (view.temperatureValid) {
    Serial.print(view.temperatureC, 1);
  } else {
    Serial.print("--");
  }
  Serial.print(" F=");
  Serial.print(view.fallState);
  Serial.print(" Thr(FF<");
  Serial.print(view.freeFallThresholdG, 2);
  Serial.print(" IMP>");
  Serial.print(view.impactThresholdG, 2);
  Serial.print(" G>");
  Serial.print(view.impactRotationThresholdDps, 0);
  Serial.print(")");
  Serial.print(" Q=");
  Serial.print(view.quality);
  Serial.print(" C=");
  Serial.print(view.signalConfidence);
  Serial.print(" H=");
  Serial.print(view.hint);
  Serial.print(" S=");
  Serial.print(view.sensorState);
  Serial.print(" LED=0x");
  if (view.ledAmplitude < 16) {
    Serial.print('0');
  }
  Serial.print(view.ledAmplitude, HEX);
  Serial.print(" M=");
  Serial.print(view.systemMode);
  Serial.print("/");
  Serial.print(view.fallProfile);
  if (view.voiceEnabled) {
    Serial.print(" V=");
    if (!view.voiceCalibrated) {
      Serial.print("CAL");
    } else {
      Serial.print(view.voiceActive ? "ON" : "OFF");
    }
    Serial.print(" VRMS=");
    Serial.print(view.voiceRms, 3);
    Serial.print(" VS=");
    Serial.print(view.voiceState);
  }
  Serial.print(" E=");
  Serial.print(view.i2cErr);
  Serial.print("/");
  Serial.print(view.mpuFail);
  Serial.print("/");
  Serial.print(view.mlxFail);
  Serial.print("/");
  Serial.print(view.micFail);
  Serial.print(" Src=");
  Serial.println(view.temperatureSource);
}

}  // namespace telemetry
}  // namespace features

