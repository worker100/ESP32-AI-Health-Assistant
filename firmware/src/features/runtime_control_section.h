#pragma once

const char* backendWifiStatusText(wl_status_t status);
void stopBackendBridge();
const char* backendVoiceStateUiText();
AiHealthContextSnapshot buildAiHealthContextSnapshot();
void logAiHealthContext(const AiHealthContextSnapshot& ctx);
void ensureSpeakerSampleRate(uint32_t sampleRate);

void refreshHrDisplayValue(float candidateBpm, uint32_t now, float alpha) {
  if (!g_vitals.heartRateDisplayValid) {
    g_vitals.heartRateDisplayBpm = candidateBpm;
    g_vitals.heartRateDisplayValid = true;
    g_vitals.lastHrDisplayRefreshMs = now;
    return;
  }

  if (!shouldRefreshStableDisplay(now, g_vitals.lastHrDisplayRefreshMs, kHrDisplayStableWindowMs)) {
    return;
  }

  g_vitals.heartRateDisplayBpm =
      (1.0f - alpha) * g_vitals.heartRateDisplayBpm + alpha * candidateBpm;
  g_vitals.lastHrDisplayRefreshMs = now;
}

void refreshSpo2DisplayValue(float candidateSpo2, uint32_t now, float alpha) {
  if (!g_vitals.spo2DisplayValid) {
    g_vitals.spo2DisplayPercent = candidateSpo2;
    g_vitals.spo2DisplayValid = true;
    g_vitals.lastSpo2DisplayRefreshMs = now;
    return;
  }

  if (!shouldRefreshStableDisplay(now, g_vitals.lastSpo2DisplayRefreshMs,
                                  kSpo2DisplayStableWindowMs)) {
    return;
  }

  g_vitals.spo2DisplayPercent =
      (1.0f - alpha) * g_vitals.spo2DisplayPercent + alpha * candidateSpo2;
  g_vitals.lastSpo2DisplayRefreshMs = now;
}

bool selectVisibleHeartRate(float& outBpm) {
  if (g_vitals.heartRateRealtimeValid) {
    outBpm = g_vitals.heartRateRealtimeBpm;
    return true;
  }
  if (g_vitals.heartRateDisplayValid) {
    outBpm = g_vitals.heartRateDisplayBpm;
    return true;
  }
  outBpm = 0.0f;
  return false;
}

bool selectVisibleSpo2(float& outPercent) {
  if (g_vitals.spo2RealtimeValid) {
    outPercent = g_vitals.spo2RealtimePercent;
    return true;
  }
  if (g_vitals.spo2DisplayValid) {
    outPercent = g_vitals.spo2DisplayPercent;
    return true;
  }
  outPercent = 0.0f;
  return false;
}

void resetMeasurementFilters() {
  g_bufferCount = 0;
  g_newSamplesSinceCalc = 0;
  g_hrHistoryIndex = 0;
  g_hrHistoryCount = 0;
  g_spo2HistoryIndex = 0;
  g_spo2HistoryCount = 0;
  g_hrValidStreak = 0;
  g_hrInvalidStreak = 0;
  g_spo2ValidStreak = 0;
  g_spo2InvalidStreak = 0;
  g_lowHrCandidateStreak = 0;
  g_spo2FallbackUseStreak = 0;
  g_lastBeatMs = 0;
  g_currentPerfusionIndex = 0.0f;
  g_lastAlgoHrBpm = 0.0f;
  g_lastAlgoHrValid = false;
  g_lastAlgoHrMs = 0;
  g_vitals.heartRateBpm = 0.0f;
  g_vitals.spo2Percent = 0.0f;
  g_vitals.heartRateDisplayBpm = 0.0f;
  g_vitals.heartRateRealtimeBpm = 0.0f;
  g_vitals.spo2DisplayPercent = 0.0f;
  g_vitals.spo2RealtimePercent = 0.0f;
  g_vitals.heartRateValid = false;
  g_vitals.spo2Valid = false;
  g_vitals.heartRateDisplayValid = false;
  g_vitals.heartRateRealtimeValid = false;
  g_vitals.spo2DisplayValid = false;
  g_vitals.spo2RealtimeValid = false;
  g_vitals.lastHrUpdateMs = 0;
  g_vitals.lastSpo2UpdateMs = 0;
  g_vitals.lastHrBeatAcceptedMs = 0;
  g_vitals.lastHrDisplayRefreshMs = 0;
  g_vitals.lastSpo2DisplayRefreshMs = 0;
}

void startCalibration(uint32_t now) {
  g_sensorState = SensorState::Calibrating;
  g_calibrationStartMs = now;
  g_calibrationPiSum = 0.0f;
  g_calibrationWindowCount = 0;
  resetMeasurementFilters();
}

const char* sensorStateText() {
  return core_text::sensorStateText(g_sensorState);
}

SignalQuality currentSignalQuality() {
  if (!g_vitals.fingerDetected) {
    return SignalQuality::NoFinger;
  }
  if (g_sensorState == SensorState::WaitingFinger) {
    return SignalQuality::Waiting;
  }
  if (g_sensorState == SensorState::Calibrating) {
    return SignalQuality::Calibrating;
  }

  const float baseline = std::max(g_calibratedPerfusionIndex, kMinPerfusionIndex);
  const float ratio = g_currentPerfusionIndex / baseline;
  if (g_vitals.heartRateValid && g_vitals.spo2Valid && ratio >= 0.75f) {
    return SignalQuality::Good;
  }
  if ((g_vitals.heartRateValid || g_vitals.spo2Valid) && ratio >= 0.35f) {
    return SignalQuality::Fair;
  }
  return SignalQuality::Poor;
}

void autoTuneLedAmplitude(uint32_t irDc, uint32_t now) {
  if (!g_vitals.fingerDetected) {
    return;
  }
  if (now - g_lastLedTuneMs < kLedTuneIntervalMs) {
    return;
  }
  g_lastLedTuneMs = now;

  uint8_t nextAmp = g_ledAmplitude;
  if (irDc < kIrTargetLow && g_ledAmplitude < kLedAmplitudeMax) {
    const uint16_t raised = static_cast<uint16_t>(g_ledAmplitude) + kLedAmplitudeStep;
    nextAmp = static_cast<uint8_t>(std::min<uint16_t>(raised, kLedAmplitudeMax));
  } else if (irDc > kIrTargetHigh && g_ledAmplitude > kLedAmplitudeMin) {
    const int lowered = static_cast<int>(g_ledAmplitude) - static_cast<int>(kLedAmplitudeStep);
    nextAmp = static_cast<uint8_t>(std::max(lowered, static_cast<int>(kLedAmplitudeMin)));
  }

  if (nextAmp != g_ledAmplitude) {
    g_ledAmplitude = nextAmp;
    max30102.setPulseAmplitudeRed(g_ledAmplitude);
    max30102.setPulseAmplitudeIR(g_ledAmplitude);
    Serial.print("LED auto-tune amp=0x");
    if (g_ledAmplitude < 16) {
      Serial.print('0');
    }
    Serial.println(g_ledAmplitude, HEX);
  }
}

void publishEvent(uint32_t eventBit) {
  g_pendingEvents |= eventBit;
}

bool isFullLogProfile() {
  return g_logProfile == LogProfile::Full;
}

void updateSystemModeAndEvents(uint32_t now) {
  const bool fallActive = (g_fallState == FallState::Alert || g_fallState == FallState::Monitor);
  const bool sensorFault = !g_mpuReady || !g_i2sReady || (kEnableMicVad && !g_micReady);

  if (sensorFault && !g_sensorFaultLatched) {
    g_sensorFaultLatched = true;
    publishEvent(kEvtSensorError);
  } else if (!sensorFault && g_sensorFaultLatched) {
    g_sensorFaultLatched = false;
    publishEvent(kEvtSensorRecovered);
  }

  if (fallActive) {
    g_systemMode = SystemMode::Alerting;
  } else if (sensorFault) {
    g_systemMode = SystemMode::Degraded;
  } else {
    g_systemMode = SystemMode::Monitoring;
  }

  if (g_pendingEvents == 0 || (now - g_lastEventLogMs) < 120) {
    return;
  }

  g_lastEventLogMs = now;
  g_lastEventMask = g_pendingEvents;
  Serial.print("EVT:");
  if (g_pendingEvents & kEvtFallFreeFall) {
    Serial.print(" FREEFALL");
  }
  if (g_pendingEvents & kEvtFallImpact) {
    Serial.print(" IMPACT");
  }
  if (g_pendingEvents & kEvtFallAlert) {
    Serial.print(" FALL_ALERT");
  }
  if (g_pendingEvents & kEvtFallCleared) {
    Serial.print(" FALL_CLEARED");
  }
  if (g_pendingEvents & kEvtFingerDetected) {
    Serial.print(" FINGER_ON");
  }
  if (g_pendingEvents & kEvtNoFinger) {
    Serial.print(" FINGER_OFF");
  }
  if (g_pendingEvents & kEvtSensorError) {
    Serial.print(" SENSOR_ERR");
  }
  if (g_pendingEvents & kEvtSensorRecovered) {
    Serial.print(" SENSOR_OK");
  }
  if (g_pendingEvents & kEvtVoiceWakeReserved) {
    Serial.print(" VOICE_WAKE");
  }
  if (g_pendingEvents & kEvtVoiceListenStart) {
    Serial.print(" VOICE_LISTEN_START");
  }
  if (g_pendingEvents & kEvtVoiceListenEnd) {
    Serial.print(" VOICE_LISTEN_END");
  }
  if (g_pendingEvents & kEvtFallProfileChanged) {
    Serial.print(" FALL_PROFILE_CHANGED");
  }
  Serial.println();
  g_pendingEvents = 0;
}

void printFallThresholds() {
  const float ff = freeFallThresholdG(g_fallProfile);
  const float imp = impactThresholdG(g_fallProfile);
  const float gthr = impactRotationThresholdDps(g_fallProfile);
  const uint32_t impWin = impactWindowMs(g_fallProfile);
  const uint32_t stillWin = stillWindowMs(g_fallProfile);

  Serial.println("Fall thresholds:");
  Serial.print("  PROFILE: ");
  Serial.println(fallProfileText(g_fallProfile));
  Serial.print("  FREEFALL: M < ");
  Serial.print(ff, 2);
  Serial.println("g");
  Serial.print("  IMPACT: M > ");
  Serial.print(imp, 2);
  Serial.print("g and Gmax > ");
  Serial.print(gthr, 0);
  Serial.println(" dps");
  Serial.print("  IMPACT window: ");
  Serial.print(impWin);
  Serial.println(" ms");
  Serial.print("  STILL: M in [");
  Serial.print(kStillAccelMinG, 2);
  Serial.print(", ");
  Serial.print(kStillAccelMaxG, 2);
  Serial.print("] g and Gmax < ");
  Serial.print(kStillGyroThresholdDps, 0);
  Serial.println(" dps");
  Serial.print("  STILL window: ");
  Serial.print(stillWin);
  Serial.println(" ms");
}

void setFallProfile(FallProfile profile, bool announce) {
  if (g_fallProfile == profile) {
    return;
  }
  g_fallProfile = profile;
  publishEvent(kEvtFallProfileChanged);
  if (announce) {
    Serial.print("Fall profile set to ");
    Serial.println(fallProfileText(g_fallProfile));
    printFallThresholds();
  }
  g_pendingPromptTone = true;
}

void setRuntimeSpeakerVolumePreset(SpeakerVolumePreset preset) {
  g_runtimeSpeakerPreset = preset;
  g_runtimeBackendTtsGain = speakerGainFromPreset(preset);
}

void pollSerialCommands(uint32_t now) {
  if ((now - g_lastSerialCmdPollMs) < kSerialCmdPollMs || !Serial.available()) {
    return;
  }
  g_lastSerialCmdPollMs = now;

  const command_parser::ParsedCommand parsed =
      command_parser::parseSerialCommand(Serial.readStringUntil('\n'));

  switch (parsed.id) {
    case command_parser::CommandId::ModeTest:
      setFallProfile(FallProfile::Test);
      break;
    case command_parser::CommandId::ModeNormal:
      setFallProfile(FallProfile::Normal);
      break;
    case command_parser::CommandId::ModeQuery:
      Serial.print("Fall profile=");
      Serial.println(fallProfileText(g_fallProfile));
      break;
    case command_parser::CommandId::VolLow:
      setRuntimeSpeakerVolumePreset(SpeakerVolumePreset::Low);
      Serial.print("Volume preset=");
      Serial.print(speakerVolumePresetText(g_runtimeSpeakerPreset));
      Serial.print(" gain=");
      Serial.println(g_runtimeBackendTtsGain, 2);
      break;
    case command_parser::CommandId::VolMedium:
      setRuntimeSpeakerVolumePreset(SpeakerVolumePreset::Medium);
      Serial.print("Volume preset=");
      Serial.print(speakerVolumePresetText(g_runtimeSpeakerPreset));
      Serial.print(" gain=");
      Serial.println(g_runtimeBackendTtsGain, 2);
      break;
    case command_parser::CommandId::VolHigh:
      setRuntimeSpeakerVolumePreset(SpeakerVolumePreset::High);
      Serial.print("Volume preset=");
      Serial.print(speakerVolumePresetText(g_runtimeSpeakerPreset));
      Serial.print(" gain=");
      Serial.println(g_runtimeBackendTtsGain, 2);
      break;
    case command_parser::CommandId::VolQuery:
      Serial.print("Volume preset=");
      Serial.print(speakerVolumePresetText(g_runtimeSpeakerPreset));
      Serial.print(" gain=");
      Serial.println(g_runtimeBackendTtsGain, 2);
      break;
    case command_parser::CommandId::LogDemo:
      g_logProfile = LogProfile::Demo;
      Serial.print("Log profile=");
      Serial.println(logProfileText(g_logProfile));
      break;
    case command_parser::CommandId::LogFull:
      g_logProfile = LogProfile::Full;
      Serial.print("Log profile=");
      Serial.println(logProfileText(g_logProfile));
      break;
    case command_parser::CommandId::LogQuery:
      Serial.print("Log profile=");
      Serial.println(logProfileText(g_logProfile));
      break;
    case command_parser::CommandId::Help:
      Serial.println(
          "Commands: MODE TEST | MODE NORMAL | MODE? | FALL? | HEALTH? | BACKEND? | "
          "BACKEND ON | BACKEND OFF | VOL LOW | VOL MED | VOL HIGH | VOL? | "
          "LOG DEMO | LOG FULL | LOG?");
      break;
    case command_parser::CommandId::HealthQuery:
      Serial.print("Health i2cErr=");
      Serial.print(g_i2cErrorCount);
      Serial.print(" mpuFail=");
      Serial.print(g_mpuReadFailStreak);
      Serial.print(" micFail=");
      Serial.print(g_micReadFailStreak);
      Serial.print(" mlxFail=");
      Serial.print(g_mlxReadFailStreak);
      Serial.print(" loopUs=");
      Serial.print(g_lastLoopDurationUs);
      Serial.print(" loopUsMax=");
      Serial.print(g_maxLoopDurationUs);
      Serial.print(" overrun=");
      Serial.println(g_loopOverrunCount);
      break;
    case command_parser::CommandId::BackendQuery:
      Serial.print("Backend enabled=");
      Serial.print(g_backendBridgeEnabled ? "ON" : "OFF");
      Serial.print(" wifi=");
      Serial.print(backendWifiStatusText(WiFi.status()));
      Serial.print(" ws=");
      Serial.print(g_backendWsReady ? "CONNECTED" : "DISCONNECTED");
      Serial.print(" hello=");
      Serial.print(g_backendHelloSent ? "SENT" : "WAIT");
      Serial.print(" voice=");
      Serial.print(g_backendVoiceSessionActive ? "STREAM"
                                               : (g_backendTtsStreaming ? "TTS"
                                                                        : (g_backendWaitingForReply ? "WAIT_REPLY" : "IDLE")));
      Serial.print(" sid=");
      Serial.print(g_backendSessionId);
      Serial.print(" vol=");
      Serial.print(speakerVolumePresetText(g_runtimeSpeakerPreset));
      Serial.print("(");
      Serial.print(g_runtimeBackendTtsGain, 2);
      Serial.println(")");
      break;
    case command_parser::CommandId::BackendOn:
      if (!g_backendBridgeEnabled) {
        g_backendBridgeEnabled = true;
        g_backendWifiStarted = false;
        g_lastBackendRetryMs = 0;
        g_backendLastStatusPushMs = 0;
        Serial.println("Backend bridge enabled");
      }
      break;
    case command_parser::CommandId::BackendOff:
      if (g_backendBridgeEnabled) {
        stopBackendBridge();
        Serial.println("Backend bridge disabled");
      }
      break;
    case command_parser::CommandId::Unknown:
    default:
      break;
  }
}

void updateHealthSnapshot(uint32_t now, uint32_t loopStartUs) {
  const uint32_t duration = micros() - loopStartUs;
  g_lastLoopDurationUs = duration;
  g_lastLoopMs = now;
  if (duration > g_maxLoopDurationUs) {
    g_maxLoopDurationUs = duration;
  }
  if (duration > 20000U) {
    ++g_loopOverrunCount;
  }

  if ((now - g_lastHealthLogMs) < kHealthLogMs) {
    return;
  }
  if (!isFullLogProfile()) {
    return;
  }
  g_lastHealthLogMs = now;
  Serial.print("HLTH loopUs=");
  Serial.print(g_lastLoopDurationUs);
  Serial.print(" maxUs=");
  Serial.print(g_maxLoopDurationUs);
  Serial.print(" i2cErr=");
  Serial.print(g_i2cErrorCount);
  Serial.print(" mpuFail=");
  Serial.print(g_mpuReadFailStreak);
  Serial.print(" mlxFail=");
  Serial.print(g_mlxReadFailStreak);
  Serial.print(" micFail=");
  Serial.print(g_micReadFailStreak);
  Serial.print(" overrun=");
  Serial.println(g_loopOverrunCount);
}

void fatalError(const char* msg) {
  Serial.println(msg);
  if (display.width() > 0) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("AI Health Monitor");
    display.println("Init Failed:");
    display.println(msg);
    display.display();
  }
  while (true) {
    delay(500);
  }
}

void renderUi() {
  const uint32_t now = millis();
  if (now - g_lastUiRefreshMs < kUiRefreshMs) {
    return;
  }
  g_lastUiRefreshMs = now;

  ui_module::OledViewModel vm;
  vm.heartRateValid = selectVisibleHeartRate(vm.heartRateBpm);
  vm.spo2Valid = selectVisibleSpo2(vm.spo2Percent);
  vm.temperatureValid = g_temperature.valid;
  vm.temperatureC = g_temperature.objectC;
  vm.fallShort = fallStateUiText(g_fallState);
  vm.guidanceShort = guidanceUiText(g_guidanceHint);
  vm.voiceShort = backendVoiceStateUiText();
  ui_module::renderOledPage(display, vm);

  if (!isFullLogProfile()) {
    return;
  }

  if (now - g_lastSerialLogMs < kSerialLogMs) {
    return;
  }
  g_lastSerialLogMs = now;

  features::telemetry::HealthLineView line;
  line.heartRateDisplayValid = g_vitals.heartRateDisplayValid;
  line.heartRateDisplayBpm = g_vitals.heartRateDisplayBpm;
  line.spo2DisplayValid = g_vitals.spo2DisplayValid;
  line.spo2DisplayPercent = g_vitals.spo2DisplayPercent;
  line.heartRateRealtimeValid = g_vitals.heartRateRealtimeValid;
  line.heartRateRealtimeBpm = g_vitals.heartRateRealtimeBpm;
  line.spo2RealtimeValid = g_vitals.spo2RealtimeValid;
  line.spo2RealtimePercent = g_vitals.spo2RealtimePercent;
  line.temperatureValid = g_temperature.valid;
  line.temperatureC = g_temperature.objectC;
  line.fallState = fallStateText(g_fallState);
  line.freeFallThresholdG = freeFallThresholdG(g_fallProfile);
  line.impactThresholdG = impactThresholdG(g_fallProfile);
  line.impactRotationThresholdDps = impactRotationThresholdDps(g_fallProfile);
  line.quality = qualityText(currentSignalQuality());
  line.signalConfidence = g_signalConfidence;
  line.hint = guidanceText(g_guidanceHint);
  line.sensorState = sensorStateText();
  line.ledAmplitude = g_ledAmplitude;
  line.systemMode = systemModeText(g_systemMode);
  line.fallProfile = fallProfileText(g_fallProfile);
  line.voiceEnabled = kEnableMicVad;
  line.voiceCalibrated = g_voiceCalibrated;
  line.voiceActive = g_voiceActive;
  line.voiceRms = g_voiceRms;
  line.voiceState = voiceStateText(g_voiceState);
  line.i2cErr = g_i2cErrorCount;
  line.mpuFail = g_mpuReadFailStreak;
  line.mlxFail = g_mlxReadFailStreak;
  line.micFail = g_micReadFailStreak;
  line.temperatureSource = g_temperature.valid ? (g_temperature.fromMax30102Die ? "MAX" : "MLX") : "NONE";
  features::telemetry::printHealthLine(line);

  const AiHealthContextSnapshot ctx = buildAiHealthContextSnapshot();
  logAiHealthContext(ctx);
}
