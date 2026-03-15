#pragma once

#include <Arduino.h>

namespace backend_log {

struct DeviceStateLogCache {
  String lastDetail;
  uint32_t lastPrintMs = 0;
};

bool shouldPrintBackendStatus(bool fullLogProfile, const char* detail, uint32_t now,
                              DeviceStateLogCache& cache);

}  // namespace backend_log

