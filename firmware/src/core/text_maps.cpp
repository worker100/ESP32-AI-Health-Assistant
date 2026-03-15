#include "text_maps.h"

namespace core_text {

const char* sensorStateText(SensorState state) {
  switch (state) {
    case SensorState::WaitingFinger:
      return "WAIT";
    case SensorState::Calibrating:
      return "CAL";
    case SensorState::Running:
      return "RUN";
  }
  return "UNK";
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

const char* speakerVolumePresetText(SpeakerVolumePreset preset) {
  switch (preset) {
    case SpeakerVolumePreset::Low:
      return "LOW";
    case SpeakerVolumePreset::Medium:
      return "MED";
    case SpeakerVolumePreset::High:
      return "HIGH";
  }
  return "UNK";
}

const char* logProfileText(LogProfile profile) {
  return (profile == LogProfile::Full) ? "FULL" : "DEMO";
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

const char* measurementConfidenceText(bool fingerDetected, SignalQuality quality, GuidanceHint hint) {
  if (!fingerDetected || quality == SignalQuality::NoFinger || quality == SignalQuality::Waiting ||
      quality == SignalQuality::Calibrating) {
    return "invalid";
  }
  if (quality == SignalQuality::Poor || hint != GuidanceHint::None) {
    return "low";
  }
  return "high";
}

const char* temperatureValidityText(const TemperatureData& temp, bool fingerDetected,
                                    SignalQuality quality, GuidanceHint hint) {
  if (!temp.valid) {
    return "unavailable";
  }
  if (temp.fromMax30102Die) {
    return "device_die_only";
  }
  if (!fingerDetected) {
    return "surface_or_environment";
  }
  if (temp.objectC < 34.5f || temp.objectC > 42.5f) {
    return "needs_recheck";
  }
  if (quality == SignalQuality::Poor || hint != GuidanceHint::None) {
    return "needs_recheck";
  }
  return "body_screening";
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

}  // namespace core_text

