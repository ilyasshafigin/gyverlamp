#include "panel_radio_binding.h"

#include <string.h>

namespace PanelRadio {

uint32_t crc32(const uint8_t* data, size_t length) {
  uint32_t value = 0xFFFFFFFFU;
  for (size_t index = 0; index < length; ++index) {
    value ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) value = (value >> 1) ^ ((value & 1U) ? 0xEDB88320U : 0U);
  }
  return value ^ 0xFFFFFFFFU;
}

bool validBinding(const Binding& binding) {
  if (!binding.bound || binding.channel == 0 || binding.channel > 14 || (binding.lampMac[0] & 1U) != 0) return false;
  for (uint8_t index = 0; index < 6; ++index) {
    if (binding.lampMac[index] != 0) return true;
  }
  return false;
}

void encodeBinding(const Binding& binding, uint8_t output[kBindingBlobSize]) {
  memset(output, 0, kBindingBlobSize);
  output[0] = kBindingMarker;
  output[1] = kBindingVersion;
  output[2] = kBindingProtocolVersion;
  output[3] = binding.bound ? kBindingFlagBound : 0;
  memcpy(output + 4, binding.lampMac, 6);
  output[10] = binding.channel;
  const uint32_t check = crc32(output, 12);
  output[12] = static_cast<uint8_t>(check);
  output[13] = static_cast<uint8_t>(check >> 8);
  output[14] = static_cast<uint8_t>(check >> 16);
  output[15] = static_cast<uint8_t>(check >> 24);
}

bool decodeBinding(const uint8_t input[kBindingBlobSize], Binding* output) {
  if (output == nullptr || input[0] != kBindingMarker || input[1] != kBindingVersion ||
      input[2] != kBindingProtocolVersion || input[3] != kBindingFlagBound || input[11] != 0) {
    return false;
  }
  const uint32_t stored = static_cast<uint32_t>(input[12]) | (static_cast<uint32_t>(input[13]) << 8) |
                          (static_cast<uint32_t>(input[14]) << 16) | (static_cast<uint32_t>(input[15]) << 24);
  Binding candidate = {true, {}, input[10]};
  memcpy(candidate.lampMac, input + 4, 6);
  if (stored != crc32(input, 12) || !validBinding(candidate)) return false;
  *output = candidate;
  return true;
}

} // namespace PanelRadio
