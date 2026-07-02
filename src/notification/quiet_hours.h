#pragma once

#include <Arduino.h>
#include "../core/quiet_hours_config.h"

struct NotificationQuietHours {
  bool enabled = DEFAULT_QUIET_ENABLED;
  uint16_t startMinutes = DEFAULT_QUIET_START_MINUTES;
  uint16_t endMinutes = DEFAULT_QUIET_END_MINUTES;

  bool isInQuietHours(uint16_t nowMinutes) const {
    // весь день
    if (startMinutes == endMinutes) return true;
    if (startMinutes < endMinutes) return nowMinutes >= startMinutes && nowMinutes < endMinutes;
    // через полночь
    return nowMinutes >= startMinutes || nowMinutes < endMinutes;
  }
};
