#pragma once

#include <Arduino.h>

#include "../config.h"
#include "../util/periodic_timer.h"

class TimeService {
public:
  TimeService();

  void init();
  void tick();
  String timeStampString() const;

  uint8_t hours() const { return hrs_; }
  uint8_t minutes() const { return mins_; }
  uint8_t seconds() const { return secs_; }
  uint8_t days() const { return days_; }

  bool isSynced() const { return timeSynced_; }
  uint16_t minutesOfDay() const { return hrs_ * 60 + mins_; }

private:
  PeriodicTimer timeTimer_;
  PeriodicTimer ntpRetryTimer_;
  uint8_t hrs_ = 0;
  uint8_t mins_ = 0;
  uint8_t secs_ = 0;
  uint8_t days_ = 0;
  uint8_t minuteCounter_ = 0;
  bool timeSynced_ = false;

  bool syncTime();
};
