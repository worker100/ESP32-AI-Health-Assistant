#pragma once

// 项目配置聚合入口（config aggregation）。
// 覆盖优先级：project_config.h（本文件） > project_local.h（可选） > project_defaults.h。
//
// 参数修改入口（常用）：
// 1) 推荐直接改本文件中的“喇叭音量三档配置（编译时）”。
// 2) 运行时也可串口临时改音量：`VOL LOW|MED|HIGH`（重启后回到编译时配置）。
// 3) 不建议直接改 `config/project_defaults.h`。

#if __has_include("config/project_local.h")
#include "config/project_local.h"
#endif

// ===== 喇叭音量三档配置（编译时）=====
// 只改下面 1 行：AI_HEALTH_CFG_SPK_PRESET
// - AI_HEALTH_SPK_PRESET_LOW
// - AI_HEALTH_SPK_PRESET_MEDIUM
// - AI_HEALTH_SPK_PRESET_HIGH
#define AI_HEALTH_SPK_PRESET_LOW 0
#define AI_HEALTH_SPK_PRESET_MEDIUM 1
#define AI_HEALTH_SPK_PRESET_HIGH 2

#ifndef AI_HEALTH_CFG_SPK_PRESET
#define AI_HEALTH_CFG_SPK_PRESET AI_HEALTH_SPK_PRESET_MEDIUM
#endif

// 三档对应增益（TTS PCM gain），需要精调时改下面三行。
#ifndef AI_HEALTH_CFG_SPK_GAIN_LOW
#define AI_HEALTH_CFG_SPK_GAIN_LOW 0.75f
#endif
#ifndef AI_HEALTH_CFG_SPK_GAIN_MEDIUM
#define AI_HEALTH_CFG_SPK_GAIN_MEDIUM 1.55f
#endif
#ifndef AI_HEALTH_CFG_SPK_GAIN_HIGH
#define AI_HEALTH_CFG_SPK_GAIN_HIGH 2.15f
#endif

// 将本文件配置映射到默认配置宏（本文件优先）。
#ifdef AI_HEALTH_SPEAKER_PRESET
#undef AI_HEALTH_SPEAKER_PRESET
#endif
#define AI_HEALTH_SPEAKER_PRESET AI_HEALTH_CFG_SPK_PRESET

#ifdef AI_HEALTH_SPK_GAIN_LOW
#undef AI_HEALTH_SPK_GAIN_LOW
#endif
#define AI_HEALTH_SPK_GAIN_LOW AI_HEALTH_CFG_SPK_GAIN_LOW

#ifdef AI_HEALTH_SPK_GAIN_MEDIUM
#undef AI_HEALTH_SPK_GAIN_MEDIUM
#endif
#define AI_HEALTH_SPK_GAIN_MEDIUM AI_HEALTH_CFG_SPK_GAIN_MEDIUM

#ifdef AI_HEALTH_SPK_GAIN_HIGH
#undef AI_HEALTH_SPK_GAIN_HIGH
#endif
#define AI_HEALTH_SPK_GAIN_HIGH AI_HEALTH_CFG_SPK_GAIN_HIGH

// ===== 显示/上报同步节奏（UI + Backend）=====
// 目标：OLED 刷新与 device_status 上报使用同一节奏，降低“同一时刻看见值不同”概率。
#ifndef AI_HEALTH_CFG_UI_REFRESH_MS
#define AI_HEALTH_CFG_UI_REFRESH_MS 120
#endif

#ifndef AI_HEALTH_CFG_BACKEND_STATUS_PUSH_MS
#define AI_HEALTH_CFG_BACKEND_STATUS_PUSH_MS AI_HEALTH_CFG_UI_REFRESH_MS
#endif

#ifndef AI_HEALTH_CFG_TEMP_READ_MS
#define AI_HEALTH_CFG_TEMP_READ_MS 500
#endif

#ifdef AI_HEALTH_UI_REFRESH_MS
#undef AI_HEALTH_UI_REFRESH_MS
#endif
#define AI_HEALTH_UI_REFRESH_MS AI_HEALTH_CFG_UI_REFRESH_MS

#ifdef AI_HEALTH_BACKEND_STATUS_PUSH_MS
#undef AI_HEALTH_BACKEND_STATUS_PUSH_MS
#endif
#define AI_HEALTH_BACKEND_STATUS_PUSH_MS AI_HEALTH_CFG_BACKEND_STATUS_PUSH_MS

#ifdef AI_HEALTH_TEMP_READ_MS
#undef AI_HEALTH_TEMP_READ_MS
#endif
#define AI_HEALTH_TEMP_READ_MS AI_HEALTH_CFG_TEMP_READ_MS

#include "config/project_defaults.h"
