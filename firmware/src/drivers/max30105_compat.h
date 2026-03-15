#pragma once

// 兼容层：SparkFun MAX3010x 库会在 MAX30105.h 里无条件重定义 I2C_BUFFER_LENGTH=32，
// 与 ESP32 Wire.h 默认 128 宏产生 redefined 警告。
// 这里先统一成 32 再包含库头，避免编译刷黄色告警。

#ifdef I2C_BUFFER_LENGTH
#undef I2C_BUFFER_LENGTH
#endif
#define I2C_BUFFER_LENGTH 32

#include "MAX30105.h"

