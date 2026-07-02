#pragma once

#include <Arduino.h>
#include "../config.h"
#include "../util/periodic_timer.h"

class TimeService {
public:
  TimeService();

  void init();
  void tick();
  String getTimeStampString() const;

  uint8_t getHours() const { return _hrs; }
  uint8_t getMinutes() const { return _mins; }
  uint8_t getSeconds() const { return _secs; }
  uint8_t getDays() const { return _days; }

  bool isSynced() const { return _timeSynced; }
  uint16_t getMinutesOfDay() const { return _hrs * 60 + _mins; }

private:
  PeriodicTimer _timeTimer;
  PeriodicTimer _ntpRetryTimer;
  uint8_t _hrs = 0;
  uint8_t _mins = 0;
  uint8_t _secs = 0;
  uint8_t _days = 0;
  uint8_t _minuteCounter = 0;
  bool _timeSynced = false;

  bool syncTime();
};
