#include "runtime_control.h"

#include <Arduino.h>
#include <WiFi.h>

#include <Adafruit_SSD1306.h>
#include "../drivers/max30105_compat.h"

#include <algorithm>

#include "../core/app_state.h"
#include "../core/runtime_constants.h"
#include "../core/text_maps.h"
#include "../modules/command_parser.h"
#include "../modules/ui_module.h"
#include "fall_profile.h"
#include "health_pipeline.h"
#include "telemetry.h"
#include "vitals_math.h"

using core_text::fallProfileText;
using core_text::fallStateText;
using core_text::fallStateUiText;
using core_text::guidanceText;
using core_text::guidanceUiText;
using core_text::logProfileText;
using core_text::qualityText;
using core_text::speakerVolumePresetText;
using core_text::systemModeText;
using core_text::voiceStateText;
using features::fall_profile::freeFallThresholdG;
using features::fall_profile::impactRotationThresholdDps;
using features::fall_profile::impactThresholdG;
using features::fall_profile::impactWindowMs;
using features::fall_profile::stillWindowMs;
using features::vitals_math::shouldRefreshStableDisplay;

#include "runtime_control_section.h"
