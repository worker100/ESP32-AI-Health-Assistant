#pragma once

#include <stddef.h>
#include <stdint.h>

namespace drivers {
namespace sensor_bus {

const char* i2cErrorText(uint8_t err);
bool i2cProbe(uint8_t addr, uint32_t& i2cErrorCount, uint8_t* errOut = nullptr);
bool mpuWriteReg8(uint8_t addr, uint8_t reg, uint8_t value);
bool mpuReadBytes(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len, uint32_t& i2cErrorCount);
bool mpuReadReg8(uint8_t addr, uint8_t reg, uint8_t* valueOut, uint32_t& i2cErrorCount);
int16_t mpuReadInt16(const uint8_t* buf, size_t offset);
bool mlxReadWordRaw(uint8_t addr, uint8_t reg, uint16_t* valueOut);
bool mlxLooksReadableAt(uint8_t addr, float* objectTempOut = nullptr, float* ambientTempOut = nullptr);

}  // namespace sensor_bus
}  // namespace drivers
