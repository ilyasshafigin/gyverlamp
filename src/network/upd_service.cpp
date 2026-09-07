#include "upd_service.h"

#ifdef USE_UDP

#include <cctype>
#include <cstdint>
#include <cstring>

#include "../core/power_controller.h"
#include "../core/state_notifier.h"
#include "../effect/controller.h"
#include "../hardware/button.h"
#include "../storage/settings_repository.h"
#include "../time/time_service.h"

namespace {
  bool hasLength(int length, size_t required) {
    return length >= 0 && static_cast<size_t>(length) >= required;
  }

  bool isDecimalDigit(char value) {
    return value >= '0' && value <= '9';
  }

  uint32_t parseDecimal(const char* value, size_t length, uint32_t maximum) {
    size_t position = 0;
    while (position < length && std::isspace(static_cast<unsigned char>(value[position]))) {
      position++;
    }

    bool negative = false;
    if (position < length && (value[position] == '+' || value[position] == '-')) {
      negative = value[position] == '-';
      position++;
    }

    uint32_t result = 0;
    while (position < length && isDecimalDigit(value[position])) {
      const uint8_t digit = static_cast<uint8_t>(value[position] - '0');
      if (result > (maximum - digit) / 10U) {
        return negative ? 0 : maximum;
      }
      result = result * 10U + digit;
      position++;
    }

    return negative ? 0 : result;
  }

  uint8_t parseUint8(const char* value, size_t length) {
    return static_cast<uint8_t>(parseDecimal(value, length, UINT8_MAX));
  }

  uint8_t parseFixedUint8(const char* value) {
    return parseUint8(value, 2);
  }
} // namespace

void UpdService::init() {
  Udp.begin(port_);
}

void UpdService::tick() {
  const int packetSize = Udp.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  char packet[kPacketBufferSize + 1];
  const int length = Udp.read(packet, kPacketBufferSize);
  if (length <= 0) {
    return;
  }

  packet[length] = '\0';

  if (Udp.remoteIP() == WiFi.localIP()) {
    return;
  }

  char reply[kReplyBufferSize + 1] = {0};
  handlePacket(packet, length, reply, sizeof(reply));
  sendReply(reply);
}

void UpdService::handlePacket(const char* packet, int length, char* reply, size_t replySize) {
  if (startsWith(packet, length, "DEB")) {
    handleDebug(reply, replySize);
  } else if (startsWith(packet, length, "GET")) {
    handleGet(reply, replySize);
  } else if (startsWith(packet, length, "EFF") && hasLength(length, 4)) {
    handleEffect(parseUint8(packet + 3, static_cast<size_t>(length - 3)), reply, replySize);
  } else if (startsWith(packet, length, "BRI") && hasLength(length, 4)) {
    handleBrightness(parseUint8(packet + 3, static_cast<size_t>(length - 3)), reply, replySize);
  } else if (startsWith(packet, length, "SPD") && hasLength(length, 4)) {
    handleSpeed(parseUint8(packet + 3, static_cast<size_t>(length - 3)), reply, replySize);
  } else if (startsWith(packet, length, "SCA") && hasLength(length, 4)) {
    handleScale(parseUint8(packet + 3, static_cast<size_t>(length - 3)), reply, replySize);
  } else if (startsWith(packet, length, "P_ON")) {
    handlePowerOn(reply, replySize);
  } else if (startsWith(packet, length, "P_OFF")) {
    handlePowerOff(reply, replySize);
  } else if (startsWith(packet, length, "ALM_SET")) {
    if (!hasLength(length, 8) || !isDecimalDigit(packet[7])) {
      return;
    }

    const uint8_t alarmIndex = static_cast<uint8_t>(packet[7] - '1');
    if (hasLength(length, 11) && memcmp(packet + 9, "ON", 2) == 0) {
      handleAlarmSet(alarmIndex, AlarmAction::Enable, 0, reply, replySize);
    } else if (hasLength(length, 12) && memcmp(packet + 9, "OFF", 3) == 0) {
      handleAlarmSet(alarmIndex, AlarmAction::Disable, 0, reply, replySize);
    } else if (hasLength(length, 10)) {
      handleAlarmSet(
        alarmIndex,
        AlarmAction::SetTime,
        static_cast<uint16_t>(parseDecimal(packet + 8, static_cast<size_t>(length - 8), UINT16_MAX)),
        reply,
        replySize
      );
    }
  } else if (startsWith(packet, length, "ALM_GET")) {
    handleAlarmGet(reply, replySize);
  } else if (startsWith(packet, length, "DAWN") && hasLength(length, 5)) {
    handleDawn(parseUint8(packet + 4, static_cast<size_t>(length - 4)), reply, replySize);
  } else if (startsWith(packet, length, "DISCOVER")) {
    handleDiscover(reply, replySize);
  } else if (startsWith(packet, length, "TMR_GET")) {
    handleTimerGet(reply, replySize);
  } else if (startsWith(packet, length, "TMR_SET") && hasLength(length, 13)) {
    const bool running = parseFixedUint8(packet + 8) != 0;
    const uint8_t option = parseFixedUint8(packet + 10);
    const uint32_t seconds = parseDecimal(packet + 12, static_cast<size_t>(length - 12), UINT32_MAX);
    handleTimerSet(running, option, seconds, reply, replySize);
  } else if (startsWith(packet, length, "FAV_GET")) {
    handleFavoritesGet(reply, replySize);
  } else if (startsWith(packet, length, "FAV_SET")) {
    handleFavoritesSet(packet, reply, replySize);
  } else if (startsWith(packet, length, "OTA")) {
    handleOta();
  } else if (startsWith(packet, length, "BTN")) {
    if (hasLength(length, 6) && memcmp(packet + 4, "ON", 2) == 0) {
      handleButton(true, reply, replySize);
    } else if (hasLength(length, 7) && memcmp(packet + 4, "OFF", 3) == 0) {
      handleButton(false, reply, replySize);
    }
  }
}

bool UpdService::startsWith(const char* packet, int length, const char* command) const {
  const size_t commandLength = strlen(command);
  return hasLength(length, commandLength) && memcmp(packet, command, commandLength) == 0;
}

void UpdService::sendReply(const char* reply) {
  if (reply[0] == '\0') {
    return;
  }

  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.write(reinterpret_cast<const uint8_t*>(reply), strlen(reply));
  Udp.endPacket();
}

void UpdService::writeTime(char* buffer, size_t bufferSize) const {
  snprintf(buffer, bufferSize, "%02u:%02u", time_.hours(), time_.minutes());
}

void UpdService::writeCurrentState(char* reply, size_t replySize) const {
  const Effects::Id effectId = effects_.selectedEffectId();
  const EffectSettings& settings = settings_.effectSettings(effectId);
  char time[6];
  writeTime(time, sizeof(time));

  snprintf(
    reply,
    replySize,
    "CURR %u %u %u %u %u %u 0 0 0 %s",
    static_cast<uint8_t>(effectId),
    settings.brightness,
    settings.speed,
    settings.scale,
    power_.isOn() ? 1 : 0,
    button_.isEnabled() ? 1 : 0,
    time
  );
}

void UpdService::handleDebug(char* reply, size_t replySize) {
  char time[6];
  writeTime(time, sizeof(time));
  snprintf(reply, replySize, "OK %s", time);
}

void UpdService::handleGet(char* reply, size_t replySize) {
  writeCurrentState(reply, replySize);
}

void UpdService::handleEffect(uint8_t effectId, char* reply, size_t replySize) {
  if (effects_.setEffect(Effects::toId(effectId))) {
    stateNotifier_.stateChanged();
  }
  writeCurrentState(reply, replySize);
}

void UpdService::handleBrightness(uint8_t brightness, char* reply, size_t replySize) {
  effects_.setEffectBrightness(brightness);
  stateNotifier_.stateChanged();
  writeCurrentState(reply, replySize);
}

void UpdService::handleSpeed(uint8_t speed, char* reply, size_t replySize) {
  effects_.setEffectSpeed(speed);
  stateNotifier_.stateChanged();
  writeCurrentState(reply, replySize);
}

void UpdService::handleScale(uint8_t scale, char* reply, size_t replySize) {
  effects_.setEffectScale(scale);
  stateNotifier_.stateChanged();
  writeCurrentState(reply, replySize);
}

void UpdService::handlePowerOn(char* reply, size_t replySize) {
  power_.on();
  writeCurrentState(reply, replySize);
}

void UpdService::handlePowerOff(char* reply, size_t replySize) {
  power_.off();
  writeCurrentState(reply, replySize);
}

void UpdService::handleAlarmSet(
  uint8_t alarmIndex, UpdService::AlarmAction action, uint16_t time, char* reply, size_t replySize
) {
  switch (action) {
    case AlarmAction::Enable: snprintf(reply, replySize, "ALM_SET%u ON", alarmIndex + 1); break;
    case AlarmAction::Disable: snprintf(reply, replySize, "ALM_SET%u OFF", alarmIndex + 1); break;
    case AlarmAction::SetTime: snprintf(reply, replySize, "ALM_SET%u %u", alarmIndex + 1, time); break;
  }
}

void UpdService::handleAlarmGet(char* reply, size_t replySize) {
  strlcpy(reply, "ALMS", replySize);
}

void UpdService::handleDawn(uint8_t dawnMode, char* reply, size_t replySize) {
  snprintf(reply, replySize, "DAWN%u", dawnMode);
}

void UpdService::handleDiscover(char* reply, size_t replySize) {
  snprintf(
    reply,
    replySize,
    "IP %u.%u.%u.%u:%u",
    WiFi.localIP()[0],
    WiFi.localIP()[1],
    WiFi.localIP()[2],
    WiFi.localIP()[3],
    port_
  );
}

void UpdService::handleTimerGet(char* reply, size_t replySize) {
  strlcpy(reply, "TMR", replySize);
}

void UpdService::handleTimerSet(bool running, uint8_t option, uint32_t seconds, char* reply, size_t replySize) {
  snprintf(reply, replySize, "TMR_SET %u %u %lu", running ? 1 : 0, option, static_cast<unsigned long>(seconds));
}

void UpdService::handleFavoritesGet(char* reply, size_t replySize) {
  strlcpy(reply, "FAV", replySize);
}

void UpdService::handleFavoritesSet(const char* packet, char* reply, size_t replySize) {
  strlcpy(reply, packet, replySize);
}

void UpdService::handleOta() {
}

void UpdService::handleButton(bool enabled, char* reply, size_t replySize) {
  button_.setEnabled(enabled);
  stateNotifier_.stateChanged();
  writeCurrentState(reply, replySize);
}

#else

void UpdService::init() {
}
void UpdService::tick() {
}

#endif
