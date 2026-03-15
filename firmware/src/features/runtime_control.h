#pragma once

#include <stdint.h>

#include "../core/domain_types.h"
#include "project_config.h"

// 运行时控制层（runtime control）：
// - Event 发布与 mode 仲裁
// - 串口命令处理
// - UI/log 渲染与运行健康统计

// 多个 feature 共用的状态辅助函数。
const char* sensorStateText();
SignalQuality currentSignalQuality();
void publishEvent(uint32_t eventBit);
bool isFullLogProfile();

// 测量状态生命周期辅助函数。
void resetMeasurementFilters();
void startCalibration(uint32_t now);
void refreshHrDisplayValue(float candidateBpm, uint32_t now, float alpha);
void refreshSpo2DisplayValue(float candidateSpo2, uint32_t now, float alpha);
void autoTuneLedAmplitude(uint32_t irDc, uint32_t now);
bool selectVisibleHeartRate(float& outBpm);
bool selectVisibleSpo2(float& outPercent);

// 顶层运行任务（top-level tasks）。
void updateSystemModeAndEvents(uint32_t now);
void printFallThresholds();
void setFallProfile(FallProfile profile, bool announce = true);

// Speaker preset 会映射到 backend TTS gain，可通过串口命令在运行时调整。
void setRuntimeSpeakerVolumePreset(SpeakerVolumePreset preset);
void pollSerialCommands(uint32_t now);
void updateHealthSnapshot(uint32_t now, uint32_t loopStartUs);
void fatalError(const char* msg);
void renderUi();
