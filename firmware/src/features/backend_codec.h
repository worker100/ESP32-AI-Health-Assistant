#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace features {
namespace backend_codec {

bool encodeBase64(const uint8_t* input, size_t inputLen, char* output, size_t outputCap);
bool decodeBase64(const String& input, uint8_t* output, size_t outputCap, size_t& outputLen);

}  // namespace backend_codec
}  // namespace features

