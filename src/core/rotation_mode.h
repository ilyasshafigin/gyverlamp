#pragma once

#include <Arduino.h>

enum class RotationMode : uint8_t {
  Off = 0,
  Sequential = 1,
  Random = 2,
};

constexpr RotationMode ROTATION_MODE_DEFAULT = RotationMode::Off;
constexpr uint16_t ROTATION_INTERVAL_SEC_DEFAULT = 60;
constexpr uint16_t ROTATION_INTERVAL_SEC_MIN = 10;
constexpr uint16_t ROTATION_INTERVAL_SEC_MAX = 3600;
constexpr uint16_t ROTATION_INTERVAL_MIN_DEFAULT = 1;
constexpr uint16_t ROTATION_INTERVAL_MIN_MIN = 1;
constexpr uint16_t ROTATION_INTERVAL_MIN_MAX = 60;
