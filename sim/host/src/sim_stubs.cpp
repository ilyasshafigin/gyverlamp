#include "time/time_service.h"
#include "hardware/led.h"

#include <cstdio>
#include <ctime>

TimeService::TimeService()
  : timeTimer_(1000),
    ntpRetryTimer_(5000) {
}

void TimeService::init() {
  syncTime();
}

bool TimeService::syncTime() {
  std::time_t now = std::time(nullptr);
  std::tm ti{};
  if (localtime_r(&now, &ti) == nullptr) return false;

  hrs_ = static_cast<uint8_t>(ti.tm_hour);
  mins_ = static_cast<uint8_t>(ti.tm_min);
  secs_ = static_cast<uint8_t>(ti.tm_sec);
  days_ = static_cast<uint8_t>(ti.tm_wday);
  minuteCounter_ = 0;
  timeSynced_ = true;
  return true;
}

void TimeService::tick() {
  syncTime();
}

String TimeService::timeStampString() const {
  char timestamp[40];
  std::snprintf(
    timestamp,
    sizeof(timestamp),
    "Date: --.--.----. Time: %02u:%02u",
    static_cast<unsigned>(hrs_),
    static_cast<unsigned>(mins_)
  );
  return String(timestamp);
}
