# ESP32 AI Health Assistant Optimization Tracker

Updated: 2026-03-15  
Rule: finish one item -> mark `[x]` immediately -> append one line to completion log.

## 1. Objectives
- Stable multi-module runtime (MAX30102 + MLX + MPU6050 + INMP441 + MAX98357 + OLED)
- Better HR/SpO2 continuity for demo use
- Fall detection that supports both real use and table-drop testing
- Observable recovery behavior when sensors fail

## 2. Tasks

### A. Architecture / Event-driven runtime
- [x] A1. Unified system mode: `BOOT / MON / ALERT / DEG`
- [x] A2. Event bus + central event logger
- [x] A3. Publish fall state transition events
- [x] A4. Publish finger on/off events
- [x] A5. Event priority arbitration (`FALL_ALERT` preempts low-priority prompt)

### B. Microphone / Voice frontend
- [x] B1. Standalone INMP441 I2S test env (RMS/Peak/MeanAbs)
- [x] B2. Mainline VAD integration (noise floor + hysteresis)
- [x] B3. `VOICE_WAKE` event in main event bus
- [x] B4. VAD debounce (min interval + consecutive frame confirm)
- [x] B5. Lightweight voice state machine (`IDLE -> LISTEN -> IDLE`)

### C. Speaker / Alarm output
- [x] C1. Suppress hiss in mic test (force GPIO4/5/6 low)
- [x] C2. Mainline non-alert mute strategy
- [x] C3. Alarm priority levels (fall alarm high priority, prompt low priority)
- [x] C4. Configurable alarm cadence by mode (`NORMAL`/`TEST`)

### D. HR/SpO2 display quality
- [x] D1. Dual-channel display (`realtime` + `stable`)
- [x] D2. HR spike/low-edge suppression (avoid 45-bpm sticking)
- [x] D3. SpO2 poor-quality strategy (gentle decay + reduced `--` flicker)
- [x] D4. Measurement guidance hint unified in OLED + serial

### E. Fall detection
- [x] E1. Print thresholds and runtime `Thr(...)`
- [x] E2. Table-test friendly tuning merged
- [x] E3. Dual fall profile: `NORMAL` / `TEST`, default `NORMAL`
- [x] E4. False-trigger suppression (confirm windows + full chain gating)

### F. Recovery / Long-run health
- [x] F1. Auto re-init on fail streak (`MPU/MAX30102/MLX/INMP441`)
- [x] F2. Unified I2C error counting + retry path
- [x] F3. Runtime health fields (loop cost, error counters, recent events)

### G. Cloud AI integration
- [x] G1. Device status uplink + AI prompt context injection
- [x] G2. Live sensor values into backend context from main program
  Notes: main firmware now extracts filtered stable health context, logs `CTX ...`, and periodically pushes structured `device_status` to the backend bridge. Backend context is cached by `device_id`, with the main health device preferred when a live voice session starts.
- [x] G3. OLED voice state presentation
- [x] G4. Merge live voice test flow into main firmware
  Notes: main firmware now supports end-to-end voice rounds (`start_session` / `audio_chunk` / `stop_session` / `interrupt` / `tts_chunk` playback) and has been validated on device with real AI replies that reference current health context.

### H. Measurement reliability / assistant safety
- [x] H1. Structured measurement confidence for AI (`high/low/invalid`)
- [x] H2. Temperature validity gating (`body_screening / needs_recheck / surface_or_environment / ...`)
- [x] H3. Louder TTS playback gain with PCM limiting
- [x] H4. Stable HR/SpO2 display hold window + timeout invalidation

### I. Firmware configuration cleanup
- [x] I1. Unified firmware config header for Wi-Fi / backend / speaker volume / voice thresholds
- [x] I2. Three speaker volume presets (`Low / Medium / High`) exposed in config header
- [x] I3. Formal main config entry moved to `firmware/include/project_config.h`
- [x] I4. Serial runtime volume command (`VOL LOW/MED/HIGH`)

### J. Codebase structure decoupling
- [x] J1. Move all test apps from `firmware/src` to `firmware/test_apps`
- [x] J2. Update PlatformIO env source filters to isolate main firmware and test apps
- [x] J3. Split backend codec and telemetry logging into `src/features`
- [x] J4. Keep main behavior equivalent while reducing inline logging/codec logic in `main.cpp`

## 3. Completion Log (newest first)
- 2026-03-15: Completed voice-context visibility and OLED overlap fixes (keep display values in `AiHealthContextSnapshot` upload path, keep confidence/validity risk labels, compact FALL area rendering), and re-verified all 8 PlatformIO env builds.
- 2026-03-15: Completed `J1/J2/J3/J4` (test code isolated into `test_apps`; env filters updated; telemetry + backend base64 codec extracted to `src/features`; main firmware behavior preserved and all env builds passed).
- 2026-03-14: Completed P0-v1 HR/SpO2 fusion upgrade (motion-aware gating, recent algo-HR agreement check for beat path, fallback SpO2 throttling, and adaptive smoothing to balance realtime feel with stability).
- 2026-03-14: Completed `G3/I4` (OLED now shows compact voice state alongside guidance; serial runtime volume commands `VOL LOW|MED|HIGH|?` added and bound to backend TTS playback gain).
- 2026-03-14: Started P0 measurement stability tuning (MAX30102 `sampleAverage 4->8`, `adcRange 4096->8192`) to reduce jitter and clipping before algorithm-structure changes.
- 2026-03-12: Completed full convergence pass for `D3/D4/E3/E4/C3/C4/B4/B5/F1/F2/F3/A5`.
- 2026-03-14: Completed `G1` (device status uplink + backend prompt context injection for AI replies).
- 2026-03-14: Completed `G2` (main firmware now sends filtered stable HR/SpO2/temperature/fall/quality context to the backend for AI session injection).
- 2026-03-14: Completed `G4` (main firmware voice path now starts backend voice sessions, streams audio, receives TTS, and has passed on-device reply tests).
- 2026-03-14: Completed `H1/H2/H3` (AI measurement confidence + temperature validity gating + louder backend TTS gain).
- 2026-03-14: Completed `H4` (added HR/SpO2 stable display windows, hold-last-valid behavior, and timeout invalidation based on last valid measurement timestamp).
- 2026-03-14: Completed `I1/I2/I3` (formal firmware config moved to `project_config.h`; Wi-Fi/backend/volume/thresholds centralized; low/medium/high presets now use practical gain values with `Low=0.35x`).
- 2026-03-12: Completed `D2` (HR low-edge and spike suppression).
- 2026-03-12: Completed `D1` (HR/O2 realtime + stable channels).
- 2026-03-12: Completed `C2` (mute speaker outside alert states).
- 2026-03-12: Completed `A1/A2/A3/A4` (mode + event bus + transition events).
- 2026-03-12: Completed `B2/B3` (mainline INMP441 VAD + `VOICE_WAKE`).
- 2026-03-12: Completed `C1` (hiss suppression in mic test).
- 2026-03-12: Completed `E1/E2` (fall threshold visibility + tuned defaults).
- 2026-03-11: Completed `B1` (standalone INMP441 test env).

## 4. Final Test Matrix
1. Static vital test (3 min): finger steady, verify no long `HR/O2=--`.
2. Wear movement test: mild hand movement, confirm guidance hints and stable channel smoothing.
3. Fall test (`NORMAL`): daily motion should not trigger alert.
4. Fall test (`TEST`): table-drop should trigger full `FREEFALL->IMPACT->ALERT`.
5. Voice test (quiet + noisy): `VOICE_WAKE` events stable, no high-frequency chatter.
6. Recovery test: disconnect/reconnect one sensor path and verify re-init + resumed output.
- 2026-03-15: Completed structural normalization pass (converted `section`-style mainline implementation into standard `*.h + *.cpp` modules for `runtime_control / device_audio / health_pipeline / backend_bridge`; `main.cpp` reduced to orchestration-only and full 8-env build regression passed).
