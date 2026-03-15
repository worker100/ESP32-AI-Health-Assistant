#include "fall_profile.h"

#include "../core/runtime_constants.h"

namespace features {
namespace fall_profile {

float freeFallThresholdG(FallProfile profile) {
  return (profile == FallProfile::Test) ? kFreeFallThresholdGTest : kFreeFallThresholdGNormal;
}

float impactThresholdG(FallProfile profile) {
  return (profile == FallProfile::Test) ? kImpactThresholdGTest : kImpactThresholdGNormal;
}

float impactRotationThresholdDps(FallProfile profile) {
  return (profile == FallProfile::Test) ? kImpactRotationThresholdDpsTest
                                        : kImpactRotationThresholdDpsNormal;
}

uint32_t impactWindowMs(FallProfile profile) {
  return (profile == FallProfile::Test) ? kImpactWindowMsTest : kImpactWindowMsNormal;
}

uint32_t stillWindowMs(FallProfile profile) {
  return (profile == FallProfile::Test) ? kStillWindowMsTest : kStillWindowMsNormal;
}

}  // namespace fall_profile
}  // namespace features

