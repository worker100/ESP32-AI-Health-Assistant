#include <Arduino.h>

#include "core/app_state.h"
#include "core/runtime_constants.h"
#include "features/backend_bridge.h"
#include "features/device_audio.h"
#include "features/health_pipeline.h"
#include "features/runtime_control.h"
#include "project_config.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Booting ESP32 AI Health Assistant...");
  // 默认 TTS 音量预设来自 project_config.h（可被 project_local.h 覆盖）。
  setRuntimeSpeakerVolumePreset(kSpeakerVolumePreset);
  g_fallProfile = FallProfile::Normal;
  printFallThresholds();
  Serial.println(
      "Cmd: MODE TEST | MODE NORMAL | MODE? | HEALTH? | BACKEND? | BACKEND ON | "
      "BACKEND OFF | VOL LOW | VOL MED | VOL HIGH | VOL? | LOG DEMO | LOG FULL | LOG?");

  initI2c();
  i2cScan();
  initOled();
  initMax30102();
  initMpu6050();
  initI2sAudio();
  initMicI2s();
  g_backendClient.onMessage(onBackendMessage);
  if (kEnableMlx90614) {
    initMlx90614(true);
  } else {
    Serial.println("MLX90614 disabled, using MAX30102 die temperature.");
  }

  g_systemMode = SystemMode::Monitoring;
  g_sensorFaultLatched = (!g_mpuReady || !g_i2sReady || (kEnableMicVad && !g_micReady));
  if (g_sensorFaultLatched) {
    publishEvent(kEvtSensorError);
  } else {
    publishEvent(kEvtSensorRecovered);
  }
}

void loop() {
  const uint32_t loopStartUs = micros();
  const uint32_t now = millis();

  // 1) 运行控制：串口命令、模式切换
  pollSerialCommands(now);

  // 2) 传感采集与派生状态
  updateSensor();
  if (now - g_lastMpuReadMs >= kMpuReadMs) {
    g_lastMpuReadMs = now;
    readMpuSample();
  }
  updateFallState(now);
  updateAlarm(now);
  updateVoiceVad(now);
  updateTemperature();

  // 3) 云端桥接 + UI/遥测日志
  updateSystemModeAndEvents(now);
  updateBackendBridge(now);
  updateBackendVoiceAssistant(now);
  renderUi();
  updateHealthSnapshot(now, loopStartUs);
}
