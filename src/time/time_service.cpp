#include "time_service.h"
#include <ESP8266WiFi.h>
#include <time.h>

namespace {
  constexpr time_t MIN_VALID_TIME = 1704067200LL; // 2024-01-01 UTC
}

TimeService::TimeService()
  : _timeTimer(1000),
    _ntpRetryTimer(5000) {
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
  if (now < MIN_VALID_TIME) {
    return false;
  }

  struct tm ti;
  if (localtime_r(&now, &ti) == nullptr) {
    return false;
  }

  _hrs = static_cast<uint8_t>(ti.tm_hour);
  _mins = static_cast<uint8_t>(ti.tm_min);
  _secs = static_cast<uint8_t>(ti.tm_sec);
  _days = static_cast<uint8_t>(ti.tm_wday);
  _minuteCounter = 0;
  _timeSynced = true;
  return true;
}

void TimeService::tick() {
  if (_timeTimer.isReady()) {
    _secs++;
    if (_secs == 60) {
      _secs = 0;
      _mins++;
      _minuteCounter++;
    }
    if (_mins == 60) {
      _mins = 0;
      _hrs++;
      if (_hrs == 24) {
        _hrs = 0;
        _days++;
        if (_days > 6) _days = 0;
      }
    }

    if (WiFi.status() == WL_CONNECTED) {
      const bool retrySync = !_timeSynced && _ntpRetryTimer.isReady();
      const bool refreshSync = _timeSynced && _minuteCounter > 30;
      if (retrySync || refreshSync) {
        syncTime();
      }
    }
  }
}

String TimeService::getTimeStampString() const {
  if (!_timeSynced) {
    return String();
  }

  time_t rawtime = time(nullptr);
  struct tm ti;
  if (rawtime < MIN_VALID_TIME || localtime_r(&rawtime, &ti) == nullptr) {
    return String();
  }

  char timestamp[40];
  snprintf(timestamp, sizeof(timestamp), "Date: %02u-%02u-%04u. Time: %02u:%02u",
    static_cast<unsigned>(ti.tm_mday),
    static_cast<unsigned>(ti.tm_mon + 1),
    static_cast<unsigned>(ti.tm_year + 1900),
    static_cast<unsigned>(ti.tm_hour),
    static_cast<unsigned>(ti.tm_min));
  return String(timestamp);
}
