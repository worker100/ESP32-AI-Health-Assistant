#pragma once

#include <stdint.h>

#include "../core/domain_types.h"

// 健康流水线层（health pipeline）：
// - MAX30102 处理（HR/SpO2）
// - 跌倒 state machine 与报警决策
// - AI context 快照生成

void updateFallState(uint32_t now);
void updateAlarm(uint32_t now);
void updateSensor();

// 构建统一 context（供串口日志与 backend 上报共用）。
AiHealthContextSnapshot buildAiHealthContextSnapshot();
void logAiHealthContext(const AiHealthContextSnapshot& ctx);
