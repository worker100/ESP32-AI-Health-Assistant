#pragma once

#include "domain_types.h"
#include "project_config.h"

namespace core_text {

const char* sensorStateText(SensorState state);
const char* qualityText(SignalQuality quality);
const char* fallStateText(FallState state);
const char* fallStateUiText(FallState state);
const char* systemModeText(SystemMode mode);
const char* fallProfileText(FallProfile profile);
const char* voiceStateText(VoiceState state);
const char* speakerVolumePresetText(SpeakerVolumePreset preset);
const char* logProfileText(LogProfile profile);
const char* guidanceText(GuidanceHint hint);
const char* measurementConfidenceText(bool fingerDetected, SignalQuality quality, GuidanceHint hint);
const char* temperatureValidityText(const TemperatureData& temp, bool fingerDetected,
                                    SignalQuality quality, GuidanceHint hint);
const char* qualityUiText(SignalQuality quality);
const char* guidanceUiText(GuidanceHint hint);

}  // namespace core_text
