#include "time/time_service.h"
#include "hardware/led.h"
#include "sim_time.h"

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
  const std::time_t now =
    sim_uses_deterministic_clock() ? static_cast<std::time_t>(sim_clock_utc_ms() / 1000ULL) : std::time(nullptr);
  std::tm ti{};
  const std::tm* converted = sim_uses_deterministic_clock() ? gmtime_r(&now, &ti) : localtime_r(&now, &ti);
  if (converted == nullptr) return false;

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
