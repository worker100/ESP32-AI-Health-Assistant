#include "backend_bridge.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <driver/i2s.h>

#include <algorithm>
#include <math.h>

#include "../core/app_state.h"
#include "../core/runtime_constants.h"
#include "../core/text_maps.h"
#include "../modules/backend_log.h"
#include "backend_codec.h"
#include "device_audio.h"
#include "health_pipeline.h"
#include "runtime_control.h"
#include "project_config.h"

using core_text::fallStateText;
using core_text::qualityText;

#include "backend_bridge_section.h"
