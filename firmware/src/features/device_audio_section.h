#pragma once

void initI2c() {
  Wire.begin(kI2cSda, kI2cScl);
  Wire.setClock(100000);
  Wire.setTimeOut(50);
}

void i2cScan() {
  Serial.println("I2C scan start...");
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("I2C device found: 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
    }
    delay(1);
  }
  Serial.println("I2C scan done.");
}

void initOled() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, kOledAddr)) {
    fatalError("OLED init failed");
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("AI Health Monitor");
  display.println("OLED initialized");
  display.display();
  delay(800);
  Serial.println("OLED initialized");
}

void initMax30102() {
  if (!max30102.begin(Wire, I2C_SPEED_STANDARD, kMax30102Addr)) {
    fatalError("MAX30102 init failed");
  }

  const byte ledBrightness = 60;
  // P0 stability tuning: stronger FIFO averaging reduces high-frequency jitter.
  const byte sampleAverage = 8;
  const byte ledMode = 2;
  const int sampleRate = 100;
  const int pulseWidth = 411;
  // Use a wider ADC full-scale to avoid clipping when perfusion increases.
  const int adcRange = 8192;

  max30102.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  g_ledAmplitude = 0x1F;
  max30102.setPulseAmplitudeRed(g_ledAmplitude);
  max30102.setPulseAmplitudeIR(g_ledAmplitude);
  max30102.setPulseAmplitudeGreen(0);
  g_lastMaxSampleMs = millis();
  g_maxSampleStallCount = 0;

  Serial.print("MAX30102 initialized cfg avg=");
  Serial.print(sampleAverage);
  Serial.print(" sr=");
  Serial.print(sampleRate);
  Serial.print(" pw=");
  Serial.print(pulseWidth);
  Serial.print(" adc=");
  Serial.println(adcRange);
}

bool initMpuAt(uint8_t addr) {
  uint8_t who = 0;
  if (!drivers::sensor_bus::i2cProbe(addr, g_i2cErrorCount) ||
      !drivers::sensor_bus::mpuReadReg8(addr, kMpuWhoAmIReg, &who, g_i2cErrorCount)) {
    return false;
  }

  Serial.print("MPU probe 0x");
  if (addr < 16) {
    Serial.print('0');
  }
  Serial.print(addr, HEX);
  Serial.print(" WHO_AM_I=0x");
  if (who < 16) {
    Serial.print('0');
  }
  Serial.println(who, HEX);

  if (who != 0x68 && who != 0x69) {
    return false;
  }

  if (!drivers::sensor_bus::mpuWriteReg8(addr, kMpuPwrMgmt1Reg, 0x00)) {
    return false;
  }
  delay(50);

  if (!drivers::sensor_bus::mpuWriteReg8(addr, kMpuAccelConfigReg, 0x00)) {
    return false;
  }
  if (!drivers::sensor_bus::mpuWriteReg8(addr, kMpuGyroConfigReg, 0x00)) {
    return false;
  }

  g_mpuReady = true;
  g_mpuAddr = addr;
  return true;
}

void initMpu6050() {
  g_mpuReady = false;
  g_mpuAddr = 0;
  g_mpuSample = {};

  if (initMpuAt(kMpuAddr0)) {
    Serial.println("MPU6050 initialized at 0x68");
    return;
  }

  if (initMpuAt(kMpuAddr1)) {
    Serial.println("MPU6050 initialized at 0x69");
    return;
  }

  Serial.println("MPU6050 init failed on 0x68/0x69");
}

void initI2sAudio() {
  const i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = kI2sSampleRate,
      .bits_per_sample = static_cast<i2s_bits_per_sample_t>(kI2sBitsPerSample),
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 8,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = true,
      .fixed_mclk = 0,
  };

  const i2s_pin_config_t pins = {
      .bck_io_num = kI2sBclkPin,
      .ws_io_num = kI2sLrcPin,
      .data_out_num = kI2sDinPin,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };

  i2s_driver_uninstall(kI2sPort);
  if (i2s_driver_install(kI2sPort, &config, 0, nullptr) == ESP_OK &&
      i2s_set_pin(kI2sPort, &pins) == ESP_OK) {
    i2s_zero_dma_buffer(kI2sPort);
    i2s_stop(kI2sPort);
    g_i2sReady = true;
    g_speakerActive = false;
    g_backendLastSpeakerRate = 0;
    Serial.println("MAX98357 I2S init OK");
    Serial.println("Speaker muted by default (non-alert mode)");
  } else {
    g_i2sReady = false;
    g_speakerActive = false;
    Serial.println("MAX98357 I2S init failed");
  }
}

void forceSpeakerPinsLow() {
  pinMode(kI2sBclkPin, OUTPUT);
  pinMode(kI2sLrcPin, OUTPUT);
  pinMode(kI2sDinPin, OUTPUT);
  digitalWrite(kI2sBclkPin, LOW);
  digitalWrite(kI2sLrcPin, LOW);
  digitalWrite(kI2sDinPin, LOW);
}

void ensureSpeakerActive() {
  if (!g_i2sReady || g_speakerActive) {
    return;
  }

  const i2s_pin_config_t pins = {
      .bck_io_num = kI2sBclkPin,
      .ws_io_num = kI2sLrcPin,
      .data_out_num = kI2sDinPin,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };
  i2s_set_pin(kI2sPort, &pins);
  i2s_zero_dma_buffer(kI2sPort);
  i2s_start(kI2sPort);
  g_speakerActive = true;
  g_backendLastSpeakerRate = 0;
}

void ensureSpeakerMuted() {
  if (!g_i2sReady || !g_speakerActive) {
    return;
  }
  i2s_zero_dma_buffer(kI2sPort);
  i2s_stop(kI2sPort);
  forceSpeakerPinsLow();
  g_speakerActive = false;
  g_backendLastSpeakerRate = 0;
}

void initMicI2s() {
  g_micReady = false;
  g_voiceActive = false;
  g_voiceCalibrated = false;
  g_voiceNoiseFloor = 0.0f;
  g_voiceRms = 0.0f;
  g_voiceOnThreshold = 0.0f;
  g_voiceOffThreshold = 0.0f;
  g_voiceStartMs = millis();
  g_voiceLastUpdateMs = 0;
  g_voiceLastActiveMs = 0;

  if (!kEnableMicVad) {
    return;
  }

  const i2s_config_t cfg = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = kMicSampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = 0,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };

  const i2s_pin_config_t pins = {
      .bck_io_num = kMicBclkPin,
      .ws_io_num = kMicWsPin,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = kMicDinPin,
  };

  i2s_driver_uninstall(kMicI2sPort);
  if (i2s_driver_install(kMicI2sPort, &cfg, 0, nullptr) == ESP_OK &&
      i2s_set_pin(kMicI2sPort, &pins) == ESP_OK) {
    i2s_zero_dma_buffer(kMicI2sPort);
    g_micReady = true;
    Serial.println("INMP441 VAD I2S init OK");
  } else {
    Serial.println("INMP441 VAD I2S init failed");
  }
}

void updateVoiceVad(uint32_t now) {
  if (!kEnableMicVad || !g_micReady || (now - g_voiceLastUpdateMs) < kMicVadUpdateMs) {
    return;
  }
  g_voiceLastUpdateMs = now;

  int32_t raw[kMicVadSamples] = {0};
  size_t bytesRead = 0;
  const esp_err_t err =
      i2s_read(kMicI2sPort, raw, sizeof(raw), &bytesRead, pdMS_TO_TICKS(12));
  if (err != ESP_OK || bytesRead == 0) {
    if (g_micReadFailStreak < 255) {
      ++g_micReadFailStreak;
    }
    if (g_micReadFailStreak >= kMicReadFailReinitThreshold) {
      Serial.println("Mic VAD read fail streak, reinit...");
      initMicI2s();
      g_micReadFailStreak = 0;
    }
    return;
  }
  g_micReadFailStreak = 0;

  const size_t samples = bytesRead / sizeof(int32_t);
  if (samples == 0) {
    return;
  }

  double sumSq = 0.0;
  for (size_t i = 0; i < samples; ++i) {
    const int32_t s24 = raw[i] >> 8;
    sumSq += static_cast<double>(s24) * static_cast<double>(s24);
  }
  g_voiceRms = sqrtf(sumSq / static_cast<float>(samples)) / 8388608.0f;

  if (!g_voiceCalibrated) {
    if (g_voiceNoiseFloor <= 0.0f) {
      g_voiceNoiseFloor = g_voiceRms;
    } else {
      g_voiceNoiseFloor =
          (1.0f - kMicVadNoiseAlpha) * g_voiceNoiseFloor + kMicVadNoiseAlpha * g_voiceRms;
    }
    if ((now - g_voiceStartMs) >= kMicVadCalMs) {
      g_voiceCalibrated = true;
    }
  } else if (!g_voiceActive) {
    g_voiceNoiseFloor =
        (1.0f - kMicVadNoiseAlpha) * g_voiceNoiseFloor + kMicVadNoiseAlpha * g_voiceRms;
  }

  g_voiceOnThreshold = std::max(kMicVadMinOn, g_voiceNoiseFloor * kMicVadOnFactor);
  g_voiceOffThreshold = std::max(kMicVadMinOff, g_voiceNoiseFloor * kMicVadOffFactor);

  const bool aboveOn = g_voiceCalibrated && (g_voiceRms >= g_voiceOnThreshold);
  const bool belowOff = g_voiceRms < g_voiceOffThreshold;
  if (aboveOn) {
    if (g_voiceOnFrameCount < 255) {
      ++g_voiceOnFrameCount;
    }
    g_voiceOffFrameCount = 0;
  } else if (belowOff) {
    if (g_voiceOffFrameCount < 255) {
      ++g_voiceOffFrameCount;
    }
    g_voiceOnFrameCount = 0;
  }

  if (!g_voiceActive && g_voiceOnFrameCount >= kMicVadConsecutiveOnFrames) {
    g_voiceActive = true;
    g_voiceLastActiveMs = now;
    if ((now - g_lastVoiceWakeMs) >= kMicVadMinTriggerIntervalMs) {
      g_lastVoiceWakeMs = now;
      publishEvent(kEvtVoiceWakeReserved);
      if (g_voiceState == VoiceState::Idle) {
        g_voiceState = VoiceState::Listen;
        g_voiceListenUntilMs = now + kVoiceListenWindowMs;
        publishEvent(kEvtVoiceListenStart);
      }
    }
  } else if (g_voiceActive) {
    if (!belowOff) {
      g_voiceLastActiveMs = now;
      g_voiceOffFrameCount = 0;
    } else if (g_voiceOffFrameCount >= kMicVadConsecutiveOffFrames &&
               now - g_voiceLastActiveMs > kMicVadOffHoldMs) {
      g_voiceActive = false;
    }
  }

  if (g_voiceState == VoiceState::Listen && now >= g_voiceListenUntilMs) {
    g_voiceState = VoiceState::Idle;
    publishEvent(kEvtVoiceListenEnd);
  }
}

void playTone(float frequencyHz, uint16_t durationMs, float amplitude = 0.35f) {
  if (!g_i2sReady || !g_speakerActive) {
    return;
  }
  ensureSpeakerSampleRate(kI2sSampleRate);

  int16_t samples[kI2sAudioBufferSamples] = {0};
  const float omega = 2.0f * PI * frequencyHz / static_cast<float>(kI2sSampleRate);
  const uint32_t totalSamples =
      static_cast<uint32_t>(kI2sSampleRate) * static_cast<uint32_t>(durationMs) / 1000U;
  uint32_t produced = 0;

  while (produced < totalSamples) {
    const size_t chunk = std::min<size_t>(kI2sAudioBufferSamples, totalSamples - produced);
    for (size_t i = 0; i < chunk; ++i) {
      const float phase = omega * static_cast<float>(produced + i);
      samples[i] = static_cast<int16_t>(sinf(phase) * amplitude * 32767.0f);
    }

    size_t written = 0;
    i2s_write(kI2sPort, samples, chunk * sizeof(int16_t), &written, portMAX_DELAY);
    produced += chunk;
  }
}

void playSilence(uint16_t durationMs) {
  if (!g_i2sReady || !g_speakerActive) {
    delay(durationMs);
    return;
  }
  ensureSpeakerSampleRate(kI2sSampleRate);

  int16_t samples[kI2sAudioBufferSamples] = {0};
  const uint32_t totalSamples =
      static_cast<uint32_t>(kI2sSampleRate) * static_cast<uint32_t>(durationMs) / 1000U;
  uint32_t produced = 0;

  while (produced < totalSamples) {
    const size_t chunk = std::min<size_t>(kI2sAudioBufferSamples, totalSamples - produced);
    size_t written = 0;
    i2s_write(kI2sPort, samples, chunk * sizeof(int16_t), &written, portMAX_DELAY);
    produced += chunk;
  }
}

void playAlarmBurst(bool monitorPhase) {
  const bool testProfile = (g_fallProfile == FallProfile::Test);
  const float first = monitorPhase ? kAlarmToneMidHz : (testProfile ? 1100.0f : kAlarmToneMidHz);
  const float second = monitorPhase ? (kAlarmToneMidHz + 260.0f) : kAlarmToneHighHz;
  const uint16_t t1 = monitorPhase ? 120 : (testProfile ? 140 : 160);
  const uint16_t t2 = monitorPhase ? 140 : (testProfile ? 160 : 180);
  playTone(first, t1);
  playSilence(60);
  playTone(second, t2);
  playSilence(testProfile ? 90 : 120);
}

void playPromptTone() {
  playTone(kPromptToneHz, 90, 0.22f);
}

void initMlx90614(bool verbose) {
  static const uint8_t kAddrCandidates[] = {0x5A, 0x5B, 0x5C, 0x5D};
  g_mlxReady = false;
  g_mlxAddrInUse = 0;
  g_temperature.valid = false;

  if (verbose) {
    Serial.println("MLX90614 probe start...");
  }

  for (uint8_t addr : kAddrCandidates) {
    uint8_t probeErr = 0;
    const bool ack = drivers::sensor_bus::i2cProbe(addr, g_i2cErrorCount, &probeErr);
    if (verbose) {
      Serial.print("  Probe 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.print(addr, HEX);
      Serial.print(": ");
      Serial.println(drivers::sensor_bus::i2cErrorText(probeErr));
    }
    if (!ack) {
      continue;
    }

    float obj = NAN;
    float amb = NAN;
    if (drivers::sensor_bus::mlxLooksReadableAt(addr, &obj, &amb)) {
      if (verbose) {
        Serial.print("    Raw read OK, Obj=");
        Serial.print(obj, 2);
        Serial.print("C, Amb=");
        Serial.print(amb, 2);
        Serial.println("C");
      }
    } else if (verbose) {
      Serial.println("    Raw read failed at this address.");
    }

    if (mlx90614.begin(addr, &Wire)) {
      g_mlxReady = true;
      g_mlxAddrInUse = addr;
      Serial.print("MLX90614 initialized at 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
      return;
    }

    if (verbose) {
      Serial.println("    Adafruit_MLX90614.begin() failed at this address.");
    }
  }

  // Fallback: in case address was changed, try to discover a readable MLX on any ACKing address.
  for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
    if (addr == kOledAddr || addr == kMax30102Addr) {
      continue;
    }
    bool isKnownCandidate = false;
    for (uint8_t candidate : kAddrCandidates) {
      if (candidate == addr) {
        isKnownCandidate = true;
        break;
      }
    }
    if (isKnownCandidate) {
      continue;
    }

    uint8_t probeErr = 0;
    if (!drivers::sensor_bus::i2cProbe(addr, g_i2cErrorCount, &probeErr)) {
      continue;
    }

    float obj = NAN;
    float amb = NAN;
    if (!drivers::sensor_bus::mlxLooksReadableAt(addr, &obj, &amb)) {
      continue;
    }

    if (verbose) {
      Serial.print("  Fallback found MLX-like response at 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.print(addr, HEX);
      Serial.print(", Obj=");
      Serial.print(obj, 2);
      Serial.print("C, Amb=");
      Serial.print(amb, 2);
      Serial.println("C");
    }

    if (mlx90614.begin(addr, &Wire)) {
      g_mlxReady = true;
      g_mlxAddrInUse = addr;
      Serial.print("MLX90614 initialized at fallback address 0x");
      if (addr < 16) {
        Serial.print('0');
      }
      Serial.println(addr, HEX);
      return;
    }
  }

  Serial.print("MLX90614 init failed (default addr 0x");
  if (kMlxDefaultAddr < 16) {
    Serial.print('0');
  }
  Serial.print(kMlxDefaultAddr, HEX);
  Serial.println(").");
  if (verbose) {
    Serial.println("Hint: if scan never shows 0x5A/0x5B/0x5C/0x5D, issue is wiring/power/module.");
  }
}

void updateTemperature() {
  const uint32_t now = millis();
  if (kEnableMlx90614 && g_mlxReady) {
    if (now - g_lastTempReadMs < kTempReadMs) {
      return;
    }
    g_lastTempReadMs = now;

    const float ambient = mlx90614.readAmbientTempC();
    const float object = mlx90614.readObjectTempC();
    if (isfinite(ambient) && isfinite(object)) {
      g_mlxReadFailStreak = 0;
      g_temperature.ambientC = ambient;
      g_temperature.objectC = object;
      g_temperature.valid = true;
      g_temperature.fromMax30102Die = false;
      g_temperature.lastUpdateMs = now;
      return;
    }

    g_temperature.valid = false;
    g_temperature.fromMax30102Die = false;
    if (g_mlxReadFailStreak < 255) {
      ++g_mlxReadFailStreak;
    }
    if (g_mlxReadFailStreak >= kMlxReadFailReinitThreshold) {
      Serial.println("MLX read unstable, reinit...");
      initMlx90614(false);
      g_mlxReadFailStreak = 0;
    }
    return;
  }

  if (kEnableMlx90614 && !g_mlxReady) {
    if (now - g_lastMlxRetryMs >= kMlxRetryMs) {
      g_lastMlxRetryMs = now;
      Serial.println("Retry MLX90614 init...");
      Wire.setClock(50000);
      initMlx90614(false);
      Wire.setClock(100000);
    }
  }

  if (!kEnableMax30102DieTemp) {
    g_temperature.valid = false;
    g_temperature.fromMax30102Die = false;
    return;
  }

  if (now - g_lastTempReadMs < kMax30102TempReadMs) {
    return;
  }
  g_lastTempReadMs = now;

  const float dieTemp = max30102.readTemperature();
  if (isfinite(dieTemp) && dieTemp > -40.0f && dieTemp < 85.0f) {
    g_temperature.ambientC = dieTemp;
    g_temperature.objectC = dieTemp;
    g_temperature.valid = true;
    g_temperature.fromMax30102Die = true;
    g_temperature.lastUpdateMs = now;
  } else {
    g_temperature.valid = false;
    g_temperature.fromMax30102Die = false;
    ++g_i2cErrorCount;
  }
}

void readMpuSample() {
  g_mpuSample.valid = false;
  if (!g_mpuReady) {
    return;
  }

  uint8_t buf[14] = {0};
  if (!drivers::sensor_bus::mpuReadBytes(g_mpuAddr, kMpuAccelStartReg, buf, sizeof(buf),
                                         g_i2cErrorCount)) {
    if (g_mpuReadFailStreak < 255) {
      ++g_mpuReadFailStreak;
    }
    if (g_mpuReadFailStreak >= kMpuReadFailReinitThreshold) {
      Serial.println("MPU read fail streak, reinit...");
      initMpu6050();
      g_mpuReadFailStreak = 0;
    }
    return;
  }
  g_mpuReadFailStreak = 0;

  const int16_t axRaw = drivers::sensor_bus::mpuReadInt16(buf, 0);
  const int16_t ayRaw = drivers::sensor_bus::mpuReadInt16(buf, 2);
  const int16_t azRaw = drivers::sensor_bus::mpuReadInt16(buf, 4);
  const int16_t gxRaw = drivers::sensor_bus::mpuReadInt16(buf, 8);
  const int16_t gyRaw = drivers::sensor_bus::mpuReadInt16(buf, 10);
  const int16_t gzRaw = drivers::sensor_bus::mpuReadInt16(buf, 12);

  g_mpuSample.axG = static_cast<float>(axRaw) / 16384.0f;
  g_mpuSample.ayG = static_cast<float>(ayRaw) / 16384.0f;
  g_mpuSample.azG = static_cast<float>(azRaw) / 16384.0f;
  g_mpuSample.gxDps = static_cast<float>(gxRaw) / 131.0f;
  g_mpuSample.gyDps = static_cast<float>(gyRaw) / 131.0f;
  g_mpuSample.gzDps = static_cast<float>(gzRaw) / 131.0f;
  g_mpuSample.accelMagG =
      sqrtf(g_mpuSample.axG * g_mpuSample.axG + g_mpuSample.ayG * g_mpuSample.ayG +
            g_mpuSample.azG * g_mpuSample.azG);
  g_mpuSample.gyroMaxDps =
      std::max(std::max(fabsf(g_mpuSample.gxDps), fabsf(g_mpuSample.gyDps)),
               fabsf(g_mpuSample.gzDps));
  g_mpuSample.valid = true;
}

