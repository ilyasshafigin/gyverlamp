#pragma once

#include <Arduino.h>

enum class RotationMode : uint8_t {
  Off = 0,
  Sequential = 1,
  Random = 2,
};

constexpr RotationMode kRotationModeDefault = RotationMode::Off;
constexpr uint16_t kRotationIntervalSecDefault = 60;
constexpr uint16_t kRotationIntervalSecMin = 10;
constexpr uint16_t kRotationIntervalSecMax = 3600;
