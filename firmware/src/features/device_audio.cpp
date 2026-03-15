#include "device_audio.h"

#include <Arduino.h>
#include <Wire.h>

#include <Adafruit_GFX.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_SSD1306.h>
#include "../drivers/max30105_compat.h"

#include <algorithm>
#include <driver/i2s.h>
#include <math.h>

#include "../core/app_state.h"
#include "../core/runtime_constants.h"
#include "../drivers/sensor_bus.h"
#include "runtime_control.h"

// Implemented in backend bridge module.
void ensureSpeakerSampleRate(uint32_t sampleRate);

#include "device_audio_section.h"
