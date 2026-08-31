#pragma once

#include <Arduino.h>
#include "../core/quiet_hours_config.h"

struct NotificationQuietHours {
  bool enabled = kDefaultQuietEnabled;
  uint16_t startMinutes = kDefaultQuietStartMinutes;
  uint16_t endMinutes = kDefaultQuietEndMinutes;

  bool isInQuietHours(uint16_t nowMinutes) const {
    // весь день
    if (startMinutes == endMinutes) return true;
    if (startMinutes < endMinutes) return nowMinutes >= startMinutes && nowMinutes < endMinutes;
    // через полночь
    return nowMinutes >= startMinutes || nowMinutes < endMinutes;
  }
};
