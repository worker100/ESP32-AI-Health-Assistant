#include "sensor_bus.h"

#include <Wire.h>
#include <math.h>

namespace drivers {
namespace sensor_bus {

const char* i2cErrorText(uint8_t err) {
  switch (err) {
    case 0:
      return "OK";
    case 1:
      return "DATA_TOO_LONG";
    case 2:
      return "ADDR_NACK";
    case 3:
      return "DATA_NACK";
    case 4:
      return "OTHER";
    case 5:
      return "TIMEOUT";
    default:
      return "UNKNOWN";
  }
}

bool i2cProbe(uint8_t addr, uint32_t& i2cErrorCount, uint8_t* errOut) {
  Wire.beginTransmission(addr);
  const uint8_t err = Wire.endTransmission();
  if (errOut != nullptr) {
    *errOut = err;
  }
  if (err != 0) {
    ++i2cErrorCount;
  }
  return err == 0;
}

bool mpuWriteReg8(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool mpuReadBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len, uint32_t& i2cErrorCount) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    ++i2cErrorCount;
    return false;
  }

  const size_t got = Wire.requestFrom(static_cast<int>(addr), static_cast<int>(len),
                                      static_cast<int>(true));
  if (got != len) {
    ++i2cErrorCount;
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    buf[i] = Wire.read();
  }
  return true;
}

bool mpuReadReg8(uint8_t addr, uint8_t reg, uint8_t* valueOut, uint32_t& i2cErrorCount) {
  return mpuReadBytes(addr, reg, valueOut, 1, i2cErrorCount);
}

int16_t mpuReadInt16(const uint8_t* buf, size_t offset) {
  return static_cast<int16_t>((static_cast<uint16_t>(buf[offset]) << 8) | buf[offset + 1]);
}

bool mlxReadWordRaw(uint8_t addr, uint8_t reg, uint16_t* valueOut) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  const uint8_t received = Wire.requestFrom(static_cast<int>(addr), 3, static_cast<int>(true));
  if (received != 3) {
    return false;
  }

  const uint8_t low = Wire.read();
  const uint8_t high = Wire.read();
  (void)Wire.read();

  if (valueOut != nullptr) {
    *valueOut = static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8);
  }
  return true;
}

bool mlxLooksReadableAt(uint8_t addr, float* objectTempOut, float* ambientTempOut) {
  uint16_t rawObj = 0;
  uint16_t rawAmb = 0;
  if (!mlxReadWordRaw(addr, 0x07, &rawObj) || !mlxReadWordRaw(addr, 0x06, &rawAmb)) {
    return false;
  }

  const float obj = static_cast<float>(rawObj) * 0.02f - 273.15f;
  const float amb = static_cast<float>(rawAmb) * 0.02f - 273.15f;

  if (!isfinite(obj) || !isfinite(amb)) {
    return false;
  }
  if (obj < -70.0f || obj > 400.0f || amb < -70.0f || amb > 200.0f) {
    return false;
  }

  if (objectTempOut != nullptr) {
    *objectTempOut = obj;
  }
  if (ambientTempOut != nullptr) {
    *ambientTempOut = amb;
  }
  return true;
}

}  // namespace sensor_bus
}  // namespace drivers
