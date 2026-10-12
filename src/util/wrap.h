#pragma once

#include <math.h>

// Coordinate wrapping fallback: invalid sizes and non-finite float inputs map to 0.
namespace CoordinateWrap {

  inline int wrap(int value, int size) {
    if (size <= 0) return 0;
    const int remainder = value % size;
    return remainder < 0 ? remainder + size : remainder;
  }

  inline float wrap(float value, int size) {
    if (size <= 0 || !isfinite(value)) return 0.0F;

    const float limit = static_cast<float>(size);
    float wrapped = fmodf(value, limit);
    if (wrapped < 0.0F) wrapped += limit;
    // Addition after fmodf can round to the upper endpoint; keep the result in [0, size).
    if (wrapped >= limit) wrapped = 0.0F;
    return wrapped;
  }

} // namespace CoordinateWrap
