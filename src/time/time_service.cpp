#include "time_service.h"
#include "../platform/wifi_headers.h"
#include <time.h>

namespace {
  constexpr time_t kMinValidTime = 1704067200LL; // 2024-01-01 UTC
}

TimeService::TimeService()
  : timeTimer_(1000),
    ntpRetryTimer_(5000) {
}

void TimeService::init() {
  configTime(GMT * 3600, 0, NTP_ADDRESS);
  randomSeed(micros());

  if (WiFi.status() == WL_CONNECTED) {
    syncTime();
  }
}

bool TimeService::syncTime() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  time_t now = time(nullptr);
  if (now < kMinValidTime) {
    return false;
  }

  struct tm ti;
  if (localtime_r(&now, &ti) == nullptr) {
    return false;
  }

  hrs_ = static_cast<uint8_t>(ti.tm_hour);
  mins_ = static_cast<uint8_t>(ti.tm_min);
  secs_ = static_cast<uint8_t>(ti.tm_sec);
  days_ = static_cast<uint8_t>(ti.tm_wday);
  minuteCounter_ = 0;
  timeSynced_ = true;
  return true;
}

void TimeService::tick() {
  if (timeTimer_.isReady()) {
    secs_++;
    if (secs_ == 60) {
      secs_ = 0;
      mins_++;
      minuteCounter_++;
    }
    if (mins_ == 60) {
      mins_ = 0;
      hrs_++;
      if (hrs_ == 24) {
        hrs_ = 0;
        days_++;
        if (days_ > 6) days_ = 0;
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      const bool retrySync = !timeSynced_ && ntpRetryTimer_.isReady();
      const bool refreshSync = timeSynced_ && minuteCounter_ > 30;
      if (retrySync || refreshSync) {
        syncTime();
      }
    }
  }
}

String TimeService::getTimeStampString() const {
  if (!timeSynced_) {
    return String();
  }

  time_t rawtime = time(nullptr);
  struct tm ti;
  if (rawtime < kMinValidTime || localtime_r(&rawtime, &ti) == nullptr) {
    return String();
  }

  char timestamp[40];
  snprintf(
    timestamp,
    sizeof(timestamp),
    "Date: %02u-%02u-%04u. Time: %02u:%02u",
    static_cast<unsigned>(ti.tm_mday),
    static_cast<unsigned>(ti.tm_mon + 1),
    static_cast<unsigned>(ti.tm_year + 1900),
    static_cast<unsigned>(ti.tm_hour),
    static_cast<unsigned>(ti.tm_min)
  );
  return String(timestamp);
}
