#pragma once

#include <stdint.h>
#include "../core/domain_types.h"

namespace features {
namespace fall_profile {

float freeFallThresholdG(FallProfile profile);
float impactThresholdG(FallProfile profile);
float impactRotationThresholdDps(FallProfile profile);
uint32_t impactWindowMs(FallProfile profile);
uint32_t stillWindowMs(FallProfile profile);

}  // namespace fall_profile
}  // namespace features
