#include "health_pipeline.h"

#include <Arduino.h>

#include "../drivers/max30105_compat.h"
#include "heartRate.h"
#include "spo2_algorithm.h"

#include <algorithm>
#include <math.h>
#include <string.h>

#include "../core/app_state.h"
#include "../core/runtime_constants.h"
#include "../core/text_maps.h"
#include "device_audio.h"
#include "fall_profile.h"
#include "runtime_control.h"
#include "telemetry.h"
#include "vitals_math.h"

using core_text::fallStateText;
using core_text::guidanceText;
using core_text::measurementConfidenceText;
using core_text::qualityText;
using core_text::temperatureValidityText;
using features::fall_profile::freeFallThresholdG;
using features::fall_profile::impactRotationThresholdDps;
using features::fall_profile::impactThresholdG;
using features::fall_profile::impactWindowMs;
using features::fall_profile::stillWindowMs;
using features::vitals_math::calcSignalConfidence;
using features::vitals_math::estimateSpo2FromWindowFallback;
using features::vitals_math::evaluateGuidance;
using features::vitals_math::pushAndMedian;

#include "health_pipeline_section.h"
