# ESP32 AI Health Assistant Optimization Tracker

Updated: 2026-03-12  
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

## 3. Completion Log (newest first)
- 2026-03-12: Completed full convergence pass for `D3/D4/E3/E4/C3/C4/B4/B5/F1/F2/F3/A5`.
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
