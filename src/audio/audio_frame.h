#pragma once

#include <Arduino.h>

struct AudioFrame {
  uint8_t level = 0;
  uint8_t bass = 0;
  uint8_t treble = 0;
  bool beat = false;
  bool available = false;
};
