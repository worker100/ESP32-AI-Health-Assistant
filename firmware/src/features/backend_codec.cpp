#include "backend_codec.h"

#include <mbedtls/base64.h>

namespace features {
namespace backend_codec {

bool encodeBase64(const uint8_t* input, size_t inputLen, char* output, size_t outputCap) {
  size_t olen = 0;
  const int rc =
      mbedtls_base64_encode(reinterpret_cast<unsigned char*>(output), outputCap, &olen, input, inputLen);
  if (rc != 0 || olen == 0 || olen >= outputCap) {
    return false;
  }
  output[olen] = '\0';
  return true;
}

bool decodeBase64(const String& input, uint8_t* output, size_t outputCap, size_t& outputLen) {
  size_t olen = 0;
  const int rc = mbedtls_base64_decode(output, outputCap, &olen,
                                       reinterpret_cast<const unsigned char*>(input.c_str()),
                                       input.length());
  if (rc != 0) {
    return false;
  }
  outputLen = olen;
  return true;
}

}  // namespace backend_codec
}  // namespace features

