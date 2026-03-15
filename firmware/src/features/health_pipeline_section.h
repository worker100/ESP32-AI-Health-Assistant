#pragma once

void updateFallState(uint32_t now) {
  if (!g_mpuSample.valid) {
    return;
  }
  const FallState prevState = g_fallState;
  const float ffThr = freeFallThresholdG(g_fallProfile);
  const float impactThr = impactThresholdG(g_fallProfile);
  const float impactRotThr = impactRotationThresholdDps(g_fallProfile);
  const uint32_t impactWinMs = impactWindowMs(g_fallProfile);
  const uint32_t stillWinMs = stillWindowMs(g_fallProfile);

  const bool stillNow = g_mpuSample.accelMagG >= kStillAccelMinG &&
                        g_mpuSample.accelMagG <= kStillAccelMaxG &&
                        g_mpuSample.gyroMaxDps <= kStillGyroThresholdDps;
  const bool recoveredNow = g_mpuSample.accelMagG >= kRecoveryAccelThresholdG ||
                            g_mpuSample.gyroMaxDps >= kRecoveryGyroThresholdDps;

  switch (g_fallState) {
    case FallState::Normal:
      if (now >= g_cooldownUntilMs && g_mpuSample.accelMagG < ffThr) {
        g_fallState = FallState::FreeFall;
        g_freeFallMs = now;
        g_impactCandidateMs = 0;
      }
      break;

    case FallState::FreeFall:
      if (now - g_freeFallMs > impactWinMs) {
        g_fallState = FallState::Normal;
      } else if (g_mpuSample.accelMagG > impactThr && g_mpuSample.gyroMaxDps > impactRotThr) {
        if (g_impactCandidateMs == 0) {
          g_impactCandidateMs = now;
        }
        if ((now - g_impactCandidateMs) >= kImpactConfirmMinMs &&
            (now - g_freeFallMs) >= kAggressiveMotionGuardMs) {
          g_fallState = FallState::Impact;
          g_impactMs = now;
          g_stillSinceMs = 0;
          g_impactCandidateMs = 0;
        }
      } else if (g_mpuSample.accelMagG > 0.90f) {
        g_fallState = FallState::Normal;
      } else {
        g_impactCandidateMs = 0;
      }
      break;

    case FallState::Impact:
      if (recoveredNow) {
        if (g_impactCandidateMs == 0) {
          g_impactCandidateMs = now;
        }
        const bool recoveryGuardPassed = (now - g_impactMs) >= kImpactRecoveryGuardMs;
        const bool recoveredConfirmed = (now - g_impactCandidateMs) >= kRecoveryConfirmMinMs;
        if (recoveryGuardPassed && recoveredConfirmed) {
          g_fallState = FallState::Normal;
          g_cooldownUntilMs = now + kFallCooldownMs;
        }
      } else if (stillNow) {
        g_impactCandidateMs = 0;
        if (g_stillSinceMs == 0) {
          g_stillSinceMs = now;
        }
        if ((now - g_impactMs) > stillWinMs && (now - g_stillSinceMs) > kStillConfirmMinMs) {
          g_fallState = FallState::Alert;
          g_alertStartMs = now;
          g_nextAlarmMs = now;
        }
      } else if (now - g_impactMs > 3000U) {
        g_fallState = FallState::Normal;
      } else {
        g_impactCandidateMs = 0;
        g_stillSinceMs = 0;
      }
      break;

    case FallState::Alert:
      if (recoveredNow) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kFallCooldownMs;
      } else if (now - g_alertStartMs >= kAlarmContinuousMs) {
        g_fallState = FallState::Monitor;
        g_nextAlarmMs = now;
      }
      break;

    case FallState::Monitor:
      if (recoveredNow) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kFallCooldownMs;
      } else if (now - g_alertStartMs >= (kAlarmContinuousMs + kAlarmMonitorMs)) {
        g_fallState = FallState::Normal;
        g_cooldownUntilMs = now + kFallCooldownMs;
      }
      break;
  }

  if (g_fallState != prevState) {
    if (g_fallState == FallState::FreeFall) {
      publishEvent(kEvtFallFreeFall);
    } else if (g_fallState == FallState::Impact) {
      publishEvent(kEvtFallImpact);
    } else if (g_fallState == FallState::Alert) {
      publishEvent(kEvtFallAlert);
    } else if (g_fallState == FallState::Normal &&
               (prevState == FallState::Alert || prevState == FallState::Monitor ||
                prevState == FallState::Impact || prevState == FallState::FreeFall)) {
      publishEvent(kEvtFallCleared);
    }
  }
}

void updateAlarm(uint32_t now) {
  if (!g_i2sReady) {
    return;
  }

  const bool alarmActive = (g_fallState == FallState::Alert || g_fallState == FallState::Monitor);
  if (!alarmActive) {
    if (g_pendingPromptTone) {
      ensureSpeakerActive();
      playPromptTone();
      g_pendingPromptTone = false;
    } else {
      ensureSpeakerMuted();
    }
    return;
  }

  // Fall alarm has highest priority and preempts any lower-priority prompt.
  g_pendingPromptTone = false;
  ensureSpeakerActive();

  if (g_fallState == FallState::Alert && now >= g_nextAlarmMs) {
    playAlarmBurst(false);
    g_nextAlarmMs = now + ((g_fallProfile == FallProfile::Test) ? kAlertBurstIntervalMsTest
                                                                 : kAlertBurstIntervalMs);
    return;
  }

  if (g_fallState == FallState::Monitor && now >= g_nextAlarmMs) {
    playAlarmBurst(true);
    g_nextAlarmMs = now + ((g_fallProfile == FallProfile::Test) ? kMonitorBurstIntervalMsTest
                                                                 : kMonitorBurstIntervalMs);
  }
}

void updateSensor() {
  bool consumedAnySample = false;
  max30102.check();

  while (max30102.available()) {
    consumedAnySample = true;
    const uint32_t now = millis();
    const uint32_t ir = max30102.getFIFOIR();
    const uint32_t red = max30102.getFIFORed();
    max30102.nextSample();
    g_lastMaxSampleMs = now;

    g_vitals.irValue = ir;
    g_vitals.redValue = red;
    const bool fingerNow = ir > kFingerDetectThreshold;
    if (fingerNow) {
      g_lastFingerSeenMs = now;
    }
    g_vitals.fingerDetected = fingerNow || ((now - g_lastFingerSeenMs) <= kFingerLostDebounceMs);
    if (g_vitals.fingerDetected != g_lastFingerEventState) {
      g_lastFingerEventState = g_vitals.fingerDetected;
      publishEvent(g_vitals.fingerDetected ? kEvtFingerDetected : kEvtNoFinger);
    }

    if (!g_vitals.fingerDetected) {
      g_sensorState = SensorState::WaitingFinger;
      g_calibrationPiSum = 0.0f;
      g_calibrationWindowCount = 0;
      g_guidanceHint = GuidanceHint::NoFinger;
      resetMeasurementFilters();
      continue;
    }

    if (g_sensorState == SensorState::WaitingFinger) {
      startCalibration(now);
    }

    if (g_bufferCount < kAlgoWindowSize) {
      g_irBuffer[g_bufferCount] = ir;
      g_redBuffer[g_bufferCount] = red;
      ++g_bufferCount;
    } else {
      memmove(g_irBuffer, g_irBuffer + 1, (kAlgoWindowSize - 1) * sizeof(uint32_t));
      memmove(g_redBuffer, g_redBuffer + 1, (kAlgoWindowSize - 1) * sizeof(uint32_t));
      g_irBuffer[kAlgoWindowSize - 1] = ir;
      g_redBuffer[kAlgoWindowSize - 1] = red;
    }
    ++g_newSamplesSinceCalc;

    if (g_bufferCount == kAlgoWindowSize && g_newSamplesSinceCalc >= kAlgoStepSamples) {
      g_newSamplesSinceCalc = 0;

      int32_t spo2 = 0;
      int8_t spo2Valid = 0;
      int32_t algoHeartRate = 0;
      int8_t algoHeartRateValid = 0;

      maxim_heart_rate_and_oxygen_saturation(
          g_irBuffer, static_cast<int32_t>(kAlgoWindowSize), g_redBuffer, &spo2, &spo2Valid,
          &algoHeartRate, &algoHeartRateValid);

      uint32_t irMin = g_irBuffer[0];
      uint32_t irMax = g_irBuffer[0];
      uint32_t redMin = g_redBuffer[0];
      uint32_t redMax = g_redBuffer[0];
      uint64_t irSum = 0;
      uint64_t redSum = 0;
      for (size_t i = 0; i < kAlgoWindowSize; ++i) {
        irMin = std::min(irMin, g_irBuffer[i]);
        irMax = std::max(irMax, g_irBuffer[i]);
        redMin = std::min(redMin, g_redBuffer[i]);
        redMax = std::max(redMax, g_redBuffer[i]);
        irSum += g_irBuffer[i];
        redSum += g_redBuffer[i];
      }

      const uint32_t irDcU32 = static_cast<uint32_t>(irSum / static_cast<uint64_t>(kAlgoWindowSize));
      const float irDc = static_cast<float>(irDcU32);
      const float irAcP2P = static_cast<float>(irMax - irMin);
      const uint32_t redDcU32 =
          static_cast<uint32_t>(redSum / static_cast<uint64_t>(kAlgoWindowSize));
      const float redAcP2P = static_cast<float>(redMax - redMin);
      const float perfusionIndex = (irDc > 0.0f) ? (irAcP2P / irDc) : 0.0f;
      g_currentPerfusionIndex = perfusionIndex;
      const float qualityRatio =
          perfusionIndex / std::max(g_calibratedPerfusionIndex, kMinPerfusionIndex);
      const bool signalQualityOk = perfusionIndex >= kMinPerfusionIndex;
      g_signalConfidence = calcSignalConfidence(qualityRatio, g_vitals.heartRateValid, g_vitals.spo2Valid);
      g_guidanceHint = evaluateGuidance(qualityRatio, perfusionIndex, g_vitals.fingerDetected,
                                        g_mpuSample.accelMagG, 0.0f, kMinPerfusionIndex);
      autoTuneLedAmplitude(irDcU32, now);

      if (g_sensorState == SensorState::Calibrating) {
        if (signalQualityOk) {
          g_calibrationPiSum += perfusionIndex;
          if (g_calibrationWindowCount < 255) {
            ++g_calibrationWindowCount;
          }
        }

        if ((now - g_calibrationStartMs) >= kCalibrationDurationMs &&
            g_calibrationWindowCount >= kCalibrationMinWindows) {
          g_calibratedPerfusionIndex =
              std::max(g_calibrationPiSum / g_calibrationWindowCount, kMinPerfusionIndex);
          g_sensorState = SensorState::Running;
        }
        continue;
      }

      const float accelDev = g_mpuSample.valid ? fabsf(g_mpuSample.accelMagG - 1.0f) : 0.0f;
      const float gyroMotion = g_mpuSample.valid ? g_mpuSample.gyroMaxDps : 0.0f;
      const bool lowMotion = !g_mpuSample.valid ||
                             (accelDev <= kHrMotionAccelDevQuiet &&
                              gyroMotion <= kHrMotionGyroQuietDps);
      const bool hardMotion = g_mpuSample.valid &&
                              (accelDev >= kHrMotionAccelDevHard ||
                               gyroMotion >= kHrMotionGyroHardDps);
      const bool stableForWindow = !hardMotion || (qualityRatio >= 0.70f);

      const bool algoHrAccepted = signalQualityOk && stableForWindow &&
                                  (qualityRatio >= kAlgoHrMinQualityRatio) &&
                                  (g_signalConfidence >= 30) &&
                                  algoHeartRateValid &&
                                  algoHeartRate >= static_cast<int32_t>(kMinAcceptedBpm) &&
                                  algoHeartRate <= static_cast<int32_t>(kMaxAcceptedBpm);
      if (algoHrAccepted) {
        const float algoBpm = static_cast<float>(algoHeartRate);
        bool jumpOk = true;
        if (g_vitals.heartRateBpm > 0.0f) {
          jumpOk = fabsf(algoBpm - g_vitals.heartRateBpm) <= (kMaxHrJumpPerUpdate + 12.0f);
        }
        if (jumpOk && g_vitals.heartRateDisplayValid) {
          const float displayDeltaLimit =
              (g_signalConfidence < kLowHrGuardMinConfidence) ? 10.0f : kAlgoHrMaxDeltaFromDisplay;
          jumpOk = fabsf(algoBpm - g_vitals.heartRateDisplayBpm) <= displayDeltaLimit;
        }

        if (jumpOk && algoBpm < kLowHrGuardFloorBpm) {
          if (g_signalConfidence < kLowHrGuardMinConfidence) {
            jumpOk = false;
          } else {
            if (g_lowHrCandidateStreak < 255) {
              ++g_lowHrCandidateStreak;
            }
            if (g_lowHrCandidateStreak < kLowHrGuardConsecutiveRequired &&
                g_vitals.heartRateDisplayValid) {
              jumpOk = false;
            }
          }
        } else if (algoBpm >= kLowHrGuardFloorBpm) {
          g_lowHrCandidateStreak = 0;
        }

        if (jumpOk) {
          const float algoHrAlpha =
              (g_signalConfidence >= 75 && lowMotion) ? 0.30f : kHrAlgoFallbackAlpha;
          if (!g_vitals.heartRateValid) {
            g_vitals.heartRateValid = true;
            g_vitals.heartRateBpm = algoBpm;
          } else {
            g_vitals.heartRateBpm =
                (1.0f - algoHrAlpha) * g_vitals.heartRateBpm + algoHrAlpha * algoBpm;
          }
          g_vitals.lastHrUpdateMs = now;
          g_lastAlgoHrValid = true;
          g_lastAlgoHrBpm = algoBpm;
          g_lastAlgoHrMs = now;

          const bool beatStale = !g_vitals.heartRateRealtimeValid ||
                                 (now - g_vitals.lastHrBeatAcceptedMs) > kBeatRealtimeStaleMs;
          if (beatStale) {
            refreshHrDisplayValue(algoBpm, now, algoHrAlpha);
          }
        }
      } else if (g_lastAlgoHrValid && (now - g_lastAlgoHrMs) > kAlgoHrFreshMs) {
        g_lastAlgoHrValid = false;
        g_lastAlgoHrBpm = 0.0f;
      }

      float fallbackSpo2 = 0.0f;
      const bool fallbackSpo2Valid = estimateSpo2FromWindowFallback(
          irDcU32, irAcP2P, redDcU32, redAcP2P, kSpo2MinIrDc, kSpo2MinRedDc, kSpo2MinAcP2P,
          &fallbackSpo2);
      const bool algoSpo2Accepted = spo2Valid && (qualityRatio >= (kSpo2MinQualityRatio * 0.5f)) &&
                                    spo2 >= kMinAcceptedSpo2 && spo2 <= kMaxAcceptedSpo2;
      const bool fallbackAllowed = lowMotion &&
                                   !hardMotion &&
                                   (qualityRatio >= kSpo2FallbackMinQualityRatio) &&
                                   (g_signalConfidence >= kSpo2FallbackMinConfidence) &&
                                   (g_spo2FallbackUseStreak < kSpo2FallbackMaxConsecutive);

      bool spo2CandidateReady = false;
      bool spo2FromAlgo = false;
      float spo2Candidate = 0.0f;
      if (algoSpo2Accepted) {
        spo2Candidate = static_cast<float>(spo2);
        spo2CandidateReady = true;
        spo2FromAlgo = true;
        g_spo2FallbackUseStreak = 0;
      } else if (fallbackSpo2Valid && fallbackAllowed) {
        spo2Candidate = fallbackSpo2;
        spo2CandidateReady = true;
        if (g_spo2FallbackUseStreak < 255) {
          ++g_spo2FallbackUseStreak;
        }
      } else {
        g_spo2FallbackUseStreak = 0;
      }

      if (spo2CandidateReady) {
        const float spo2Median = pushAndMedian(spo2Candidate, g_spo2History, &g_spo2HistoryIndex,
                                               &g_spo2HistoryCount, kMedianWindowSize);

        bool jumpOk = true;
        if (g_vitals.spo2Percent > 0.0f) {
          float limit = g_vitals.spo2Valid
                            ? (spo2FromAlgo ? kMaxSpo2JumpPerUpdate : kMaxSpo2FallbackJumpPerUpdate)
                            : kMaxSpo2ReacquireJump;
          if (g_vitals.spo2Valid && qualityRatio < 0.25f) {
            limit += 1.5f;
          }
          jumpOk = fabsf(spo2Median - g_vitals.spo2Percent) <= limit;
        }

        if (jumpOk) {
          g_spo2InvalidStreak = 0;
          if (g_spo2ValidStreak < 255) {
            ++g_spo2ValidStreak;
          }

          if (!g_vitals.spo2Valid) {
            if (g_spo2ValidStreak >= kSpo2ValidStreakRequired) {
              g_vitals.spo2Valid = true;
              g_vitals.spo2Percent = spo2Median;
              g_vitals.lastSpo2UpdateMs = now;
            }
          } else {
            const float spo2Alpha =
                spo2FromAlgo
                    ? ((g_signalConfidence >= 70 && lowMotion) ? 0.28f : kSpo2EwmaAlpha)
                    : 0.14f;
            g_vitals.spo2Percent =
                (1.0f - spo2Alpha) * g_vitals.spo2Percent + spo2Alpha * spo2Median;
            g_vitals.lastSpo2UpdateMs = now;
          }

          float spo2Realtime = spo2Candidate;
          if (g_vitals.spo2RealtimeValid) {
            const float deltaSpo2 = spo2Realtime - g_vitals.spo2RealtimePercent;
            if (fabsf(deltaSpo2) > kSpo2RealtimeMaxJump) {
              spo2Realtime = g_vitals.spo2RealtimePercent +
                             ((deltaSpo2 > 0.0f) ? kSpo2RealtimeMaxJump : -kSpo2RealtimeMaxJump);
            }
          }
          g_vitals.spo2RealtimePercent = spo2Realtime;
          g_vitals.spo2RealtimeValid = true;

          const float spo2DisplayAlpha =
              spo2FromAlgo ? ((g_signalConfidence >= 70 && lowMotion) ? 0.30f : kSpo2DisplayAlpha)
                           : 0.16f;
          refreshSpo2DisplayValue(spo2Realtime, now, spo2DisplayAlpha);
        } else {
          g_spo2ValidStreak = 0;
          if (g_spo2InvalidStreak < 255) {
            g_spo2InvalidStreak += (qualityRatio < 0.3f) ? 1 : 2;
            if (g_spo2InvalidStreak > 255) {
              g_spo2InvalidStreak = 255;
            }
          }
        }
      } else {
        g_spo2ValidStreak = 0;
        if (g_spo2InvalidStreak < 255) {
          g_spo2InvalidStreak += (qualityRatio < 0.3f) ? 1 : 2;
          if (g_spo2InvalidStreak > 255) {
            g_spo2InvalidStreak = 255;
          }
        }
      }

      if (g_guidanceHint == GuidanceHint::None) {
        g_guidanceHint = evaluateGuidance(qualityRatio, perfusionIndex, g_vitals.fingerDetected,
                                          g_mpuSample.accelMagG,
                                          spo2CandidateReady ? spo2Candidate : 0.0f,
                                          kMinPerfusionIndex);
      }

      if (g_spo2InvalidStreak >= kInvalidStreakDrop &&
          (now - g_vitals.lastSpo2UpdateMs) > kSpo2DisplayHoldMs) {
        g_vitals.spo2Valid = false;
        g_vitals.spo2Percent = 0.0f;
        g_vitals.spo2DisplayValid = false;
        g_vitals.spo2RealtimeValid = false;
        g_vitals.spo2DisplayPercent = 0.0f;
        g_vitals.spo2RealtimePercent = 0.0f;
      }
    }

    if (g_sensorState == SensorState::Running && checkForBeat(ir)) {
      if (g_lastBeatMs > 0) {
        const uint32_t delta = now - g_lastBeatMs;
        if (delta > 0) {
          const float bpm = 60000.0f / static_cast<float>(delta);
          const float beatAccelDev = g_mpuSample.valid ? fabsf(g_mpuSample.accelMagG - 1.0f) : 0.0f;
          const float beatGyro = g_mpuSample.valid ? g_mpuSample.gyroMaxDps : 0.0f;
          const bool beatLowMotion = !g_mpuSample.valid ||
                                     (beatAccelDev <= kHrMotionAccelDevQuiet &&
                                      beatGyro <= kHrMotionGyroQuietDps);
          const bool beatHardMotion = g_mpuSample.valid &&
                                      (beatAccelDev >= kHrMotionAccelDevHard ||
                                       beatGyro >= kHrMotionGyroHardDps);

          bool accepted = bpm >= kMinAcceptedBpm && bpm <= kMaxAcceptedBpm;
          if (accepted && beatHardMotion && g_signalConfidence < 70) {
            accepted = false;
          }
          if (accepted && g_vitals.heartRateValid &&
              fabsf(bpm - g_vitals.heartRateBpm) > kMaxHrJumpPerUpdate) {
            accepted = false;
          }
          if (accepted && g_lastAlgoHrValid && (now - g_lastAlgoHrMs) <= kAlgoHrFreshMs) {
            const float agreeLimit =
                beatLowMotion ? kHrBeatAlgoAgreeLowMotionBpm : kHrBeatAlgoAgreeHighMotionBpm;
            if (fabsf(bpm - g_lastAlgoHrBpm) > agreeLimit) {
              accepted = false;
            }
          }
          if (accepted && g_vitals.heartRateDisplayValid) {
            const float displayDeltaLimit =
                (g_signalConfidence < kLowHrGuardMinConfidence) ? 10.0f : kMaxHrJumpPerUpdate;
            if (fabsf(bpm - g_vitals.heartRateDisplayBpm) > displayDeltaLimit) {
              accepted = false;
            }
          }
          if (accepted && bpm < kLowHrGuardFloorBpm) {
            if (g_signalConfidence < kLowHrGuardMinConfidence) {
              accepted = false;
            } else {
              if (g_lowHrCandidateStreak < 255) {
                ++g_lowHrCandidateStreak;
              }
              if (g_lowHrCandidateStreak < kLowHrGuardConsecutiveRequired &&
                  g_vitals.heartRateDisplayValid) {
                accepted = false;
              }
            }
          } else if (bpm >= kLowHrGuardFloorBpm) {
            g_lowHrCandidateStreak = 0;
          }

          if (accepted) {
            const float hrMedian = pushAndMedian(bpm, g_hrHistory, &g_hrHistoryIndex,
                                                 &g_hrHistoryCount, kMedianWindowSize);
            g_hrInvalidStreak = 0;
            if (g_hrValidStreak < 255) {
              ++g_hrValidStreak;
            }

            if (!g_vitals.heartRateValid) {
              if (g_hrValidStreak >= kHrValidStreakRequired) {
                g_vitals.heartRateValid = true;
                g_vitals.heartRateBpm = hrMedian;
                g_vitals.lastHrUpdateMs = now;
              }
            } else {
              const float hrAlpha =
                  (g_signalConfidence >= 70 && beatLowMotion) ? kHrEwmaAlpha : 0.20f;
              g_vitals.heartRateBpm =
                  (1.0f - hrAlpha) * g_vitals.heartRateBpm + hrAlpha * hrMedian;
              g_vitals.lastHrUpdateMs = now;
            }

            float realtime = bpm;
            if (g_vitals.heartRateRealtimeValid) {
              const float deltaHr = realtime - g_vitals.heartRateRealtimeBpm;
              if (fabsf(deltaHr) > kHrRealtimeMaxJump) {
                realtime = g_vitals.heartRateRealtimeBpm +
                           ((deltaHr > 0.0f) ? kHrRealtimeMaxJump : -kHrRealtimeMaxJump);
              }
            }
            g_vitals.heartRateRealtimeBpm = realtime;
            g_vitals.heartRateRealtimeValid = true;

            const float hrDisplayAlpha =
                (g_signalConfidence >= 70 && beatLowMotion) ? kHrDisplayAlpha : 0.20f;
            refreshHrDisplayValue(realtime, now, hrDisplayAlpha);
            g_vitals.lastHrBeatAcceptedMs = now;
          } else {
            g_hrValidStreak = 0;
            if (g_hrInvalidStreak < 255) {
              ++g_hrInvalidStreak;
            }
          }
        }
      }
      g_lastBeatMs = now;
    }
  }

  const uint32_t now = millis();
  if (!consumedAnySample && (now - g_lastMaxSampleMs) > kMaxSampleStallMs) {
    if (g_maxSampleStallCount < 255) {
      ++g_maxSampleStallCount;
    }
    if (g_maxSampleStallCount >= kMaxSampleStallReinitThreshold) {
      Serial.println("MAX30102 sample stall, reinit...");
      initMax30102();
      g_maxSampleStallCount = 0;
      publishEvent(kEvtSensorRecovered);
    }
    g_lastMaxSampleMs = now;
  }

  if (g_vitals.fingerDetected && g_vitals.heartRateValid &&
      (millis() - g_vitals.lastHrUpdateMs) > kBeatTimeoutMs) {
    g_vitals.heartRateValid = false;
  }

  if (g_vitals.fingerDetected && g_vitals.heartRateDisplayValid &&
      g_vitals.lastHrUpdateMs > 0 &&
      (millis() - g_vitals.lastHrUpdateMs) > kHrDisplayHoldMs) {
    g_vitals.heartRateDisplayValid = false;
    g_vitals.heartRateRealtimeValid = false;
    g_vitals.heartRateDisplayBpm = 0.0f;
    g_vitals.heartRateRealtimeBpm = 0.0f;
  }

  if (g_vitals.fingerDetected && g_vitals.spo2Valid &&
      (millis() - g_vitals.lastSpo2UpdateMs) > kBeatTimeoutMs) {
    g_vitals.spo2Valid = false;
    g_vitals.spo2Percent = 0.0f;
  }

  if (g_vitals.fingerDetected && g_vitals.spo2DisplayValid &&
      g_vitals.lastSpo2UpdateMs > 0 &&
      (millis() - g_vitals.lastSpo2UpdateMs) > kSpo2DisplayHoldMs) {
    g_vitals.spo2DisplayValid = false;
    g_vitals.spo2RealtimeValid = false;
    g_vitals.spo2DisplayPercent = 0.0f;
    g_vitals.spo2RealtimePercent = 0.0f;
  }
}
AiHealthContextSnapshot buildAiHealthContextSnapshot() {
  AiHealthContextSnapshot ctx;
  ctx.heartRateValid = selectVisibleHeartRate(ctx.heartRateBpm);
  ctx.spo2Valid = selectVisibleSpo2(ctx.spo2Percent);
  ctx.temperatureValid = g_temperature.valid;
  ctx.temperatureC = g_temperature.objectC;
  ctx.fingerDetected = g_vitals.fingerDetected;
  ctx.signalQuality = currentSignalQuality();
  ctx.fallState = g_fallState;
  ctx.guidanceHint = g_guidanceHint;
  ctx.measurementConfidence =
      measurementConfidenceText(ctx.fingerDetected, ctx.signalQuality, ctx.guidanceHint);
  ctx.temperatureValidity =
      temperatureValidityText(g_temperature, ctx.fingerDetected, ctx.signalQuality, ctx.guidanceHint);

  // 保留 display values 上报给后端，避免 AI 因字段缺失而无法参考当前读数。
  // 由 measurement_confidence / temperature_validity 负责标注是否可靠。
  if (g_temperature.valid) {
    ctx.tempSource = g_temperature.fromMax30102Die ? "MAX" : "MLX";
  }
  return ctx;
}

void logAiHealthContext(const AiHealthContextSnapshot& ctx) {
  features::telemetry::ContextLogView view;
  view.heartRateValid = ctx.heartRateValid;
  view.heartRateBpm = ctx.heartRateBpm;
  view.spo2Valid = ctx.spo2Valid;
  view.spo2Percent = ctx.spo2Percent;
  view.temperatureValid = ctx.temperatureValid;
  view.temperatureC = ctx.temperatureC;
  view.fingerDetected = ctx.fingerDetected;
  view.quality = qualityText(ctx.signalQuality);
  view.fall = fallStateText(ctx.fallState);
  view.hint = guidanceText(ctx.guidanceHint);
  view.confidence = ctx.measurementConfidence;
  view.temperatureValidity = ctx.temperatureValidity;
  view.temperatureSource = ctx.tempSource;
  features::telemetry::printContextLine(view);
}



