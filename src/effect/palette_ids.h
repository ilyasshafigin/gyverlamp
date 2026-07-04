#pragma once

#include <cstdint>

namespace Palettes {

  enum class Id : uint8_t {
    Auto = 0,
    Rainbow,
    Party,
    Ocean,
    Lava,
    Heat,
    Forest,
    Clouds,
    Sunset,
    OptimusPrime,
    Warm,
    Cold,
    Hot,
    Pink,
    Comfy,
    Cyberpank,
    Xmas,
    Acid,
    Gummy,
  };

  constexpr uint8_t toIndex(Id id) {
    return static_cast<uint8_t>(id);
  }

} // namespace Palettes
