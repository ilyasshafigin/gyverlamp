#include "time/time_service.h"
#include "hardware/led.h"

#include <cstdio>
#include <ctime>

TimeService::TimeService() :
    _timeTimer(1000),
    _ntpRetryTimer(5000) {
}

void TimeService::init() {
    syncTime();
}

bool TimeService::syncTime() {
    std::time_t now = std::time(nullptr);
    std::tm ti{};
    if (localtime_r(&now, &ti) == nullptr) return false;

    _hrs = static_cast<uint8_t>(ti.tm_hour);
    _mins = static_cast<uint8_t>(ti.tm_min);
    _secs = static_cast<uint8_t>(ti.tm_sec);
    _days = static_cast<uint8_t>(ti.tm_wday);
    _minuteCounter = 0;
    _timeSynced = true;
    return true;
}

void TimeService::tick() {
    syncTime();
}

String TimeService::getTimeStampString() const {
    char timestamp[40];
    std::snprintf(timestamp, sizeof(timestamp), "Date: --.--.----. Time: %02u:%02u",
        static_cast<unsigned>(_hrs),
        static_cast<unsigned>(_mins));
    return String(timestamp);
}
