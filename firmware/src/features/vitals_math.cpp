#include "vitals_math.h"

#include <algorithm>
#include <math.h>

namespace features {
namespace vitals_math {

float medianFromHistory(const float* history, uint8_t count) {
  if (count == 0) {
    return 0.0f;
  }

  float scratch[8] = {0};
  for (uint8_t i = 0; i < count; ++i) {
    scratch[i] = history[i];
  }
  std::sort(scratch, scratch + count);

  if ((count & 1U) == 1U) {
    return scratch[count / 2];
  }
  return (scratch[(count / 2) - 1] + scratch[count / 2]) * 0.5f;
}

float pushAndMedian(float value, float* history, uint8_t* index, uint8_t* count, uint8_t windowSize) {
  history[*index] = value;
  *index = static_cast<uint8_t>((*index + 1) % windowSize);
  if (*count < windowSize) {
    *count += 1;
  }
  return medianFromHistory(history, *count);
}

bool shouldRefreshStableDisplay(uint32_t now, uint32_t lastRefreshMs, uint32_t stableWindowMs) {
  if (lastRefreshMs == 0 || stableWindowMs == 0) {
    return true;
  }
  return (now - lastRefreshMs) >= stableWindowMs;
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
                              float accelMag, float spo2Candidate, float minPerfusionIndex) {
  if (!fingerDetected) {
    return GuidanceHint::NoFinger;
  }
  if (qualityRatio < 0.22f || perfusionIndex < (minPerfusionIndex * 0.8f)) {
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

bool estimateSpo2FromWindowFallback(uint32_t irDc, float irAcP2P, uint32_t redDc, float redAcP2P,
                                    uint32_t minIrDc, uint32_t minRedDc, float minAcP2P,
                                    float* spo2Out) {
  if (irDc < minIrDc || redDc < minRedDc || irAcP2P < minAcP2P || redAcP2P < minAcP2P) {
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

}  // namespace vitals_math
}  // namespace features

