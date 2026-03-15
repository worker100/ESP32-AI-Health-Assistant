#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../core/domain_types.h"

namespace features {
namespace vitals_math {

float medianFromHistory(const float* history, uint8_t count);
float pushAndMedian(float value, float* history, uint8_t* index, uint8_t* count, uint8_t windowSize);
bool shouldRefreshStableDisplay(uint32_t now, uint32_t lastRefreshMs, uint32_t stableWindowMs);
uint8_t clampConfidence(int value);
uint8_t calcSignalConfidence(float qualityRatio, bool hrValid, bool spo2Valid);
GuidanceHint evaluateGuidance(float qualityRatio, float perfusionIndex, bool fingerDetected,
                              float accelMag, float spo2Candidate, float minPerfusionIndex);
bool estimateSpo2FromWindowFallback(uint32_t irDc, float irAcP2P, uint32_t redDc, float redAcP2P,
                                    uint32_t minIrDc, uint32_t minRedDc, float minAcP2P,
                                    float* spo2Out);

}  // namespace vitals_math
}  // namespace features
