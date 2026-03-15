#include "backend_log.h"

#include <string.h>

namespace backend_log {
namespace {
constexpr uint32_t kDeviceStateRepeatLogMinMs = 2000;
}

bool shouldPrintBackendStatus(bool fullLogProfile, const char* detail, uint32_t now,
                              DeviceStateLogCache& cache) {
  const bool noisyDeviceState = (detail != nullptr) && (strncmp(detail, "device_state=", 13) == 0);
  if (!noisyDeviceState) {
    return true;
  }

  if (!fullLogProfile) {
    return false;
  }

  const String current = detail;
  const bool unchanged = (current == cache.lastDetail);
  if (unchanged && (now - cache.lastPrintMs) < kDeviceStateRepeatLogMinMs) {
    return false;
  }

  cache.lastDetail = current;
  cache.lastPrintMs = now;
  return true;
}

}  // namespace backend_log

