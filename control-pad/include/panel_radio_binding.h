#pragma once

#include <stddef.h>
#include <stdint.h>

namespace PanelRadio {

constexpr size_t kBindingBlobSize = 16;
constexpr uint8_t kBindingMarker = 0xC2;
constexpr uint8_t kBindingVersion = 1;
constexpr uint8_t kBindingProtocolVersion = 1;
constexpr uint8_t kBindingFlagBound = 0x01;

struct Binding {
  bool bound;
  uint8_t lampMac[6];
  uint8_t channel;
};

uint32_t crc32(const uint8_t* data, size_t length);
bool validBinding(const Binding& binding);
void encodeBinding(const Binding& binding, uint8_t output[kBindingBlobSize]);
bool decodeBinding(const uint8_t input[kBindingBlobSize], Binding* output);

class BindingStore {
 public:
  virtual ~BindingStore() {}
  virtual bool load(Binding* output) = 0;
  virtual bool write(const uint8_t blob[kBindingBlobSize]) = 0;
};

} // namespace PanelRadio
