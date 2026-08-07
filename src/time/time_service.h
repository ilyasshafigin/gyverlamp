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

  uint8_t getHours() const { return hrs_; }
  uint8_t getMinutes() const { return mins_; }
  uint8_t getSeconds() const { return secs_; }
  uint8_t getDays() const { return days_; }

  bool isSynced() const { return timeSynced_; }
  uint16_t getMinutesOfDay() const { return hrs_ * 60 + mins_; }

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
