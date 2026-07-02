#include "upd_service.h"

#ifdef USE_UDP

#include "../core/power_controller.h"
#include "../core/state_notifier.h"
#include "../effect/controller.h"
#include "../hardware/button.h"
#include "../storage/settings_repository.h"
#include "../time/time_service.h"


namespace {
  uint8_t parseUint8(const char* value) {
    return static_cast<uint8_t>(constrain(atoi(value), 0, 255));
  }

  uint8_t parseFixedUint8(const char* value) {
    char buffer[3] = { value[0], value[1], '\0' };
    return parseUint8(buffer);
  }
}

void UpdService::init() {
  Udp.begin(_port);
}

void UpdService::tick() {
  const int packetSize = Udp.parsePacket();
  if (packetSize <= 0) {
    return;
  }

  char packet[PACKET_BUFFER_SIZE + 1];
  const int length = Udp.read(packet, PACKET_BUFFER_SIZE);
  if (length <= 0) {
    return;
  }

  packet[length] = '\0';

  if (Udp.remoteIP() == WiFi.localIP()) {
    return;
  }

  char reply[REPLY_BUFFER_SIZE + 1] = { 0 };
  handlePacket(packet, length, reply, sizeof(reply));
  sendReply(reply);
}

void UpdService::handlePacket(const char* packet, int length, char* reply, size_t replySize) {
  (void)length;

  if (startsWith(packet, "DEB")) {
    handleDebug(reply, replySize);
  } else if (startsWith(packet, "GET")) {
    handleGet(reply, replySize);
  } else if (startsWith(packet, "EFF")) {
    handleEffect(parseUint8(packet + 3), reply, replySize);
  } else if (startsWith(packet, "BRI")) {
    handleBrightness(parseUint8(packet + 3), reply, replySize);
  } else if (startsWith(packet, "SPD")) {
    handleSpeed(parseUint8(packet + 3), reply, replySize);
  } else if (startsWith(packet, "SCA")) {
    handleScale(parseUint8(packet + 3), reply, replySize);
  } else if (startsWith(packet, "P_ON")) {
    handlePowerOn(reply, replySize);
  } else if (startsWith(packet, "P_OFF")) {
    handlePowerOff(reply, replySize);
  } else if (startsWith(packet, "ALM_SET")) {
    const uint8_t alarmIndex = static_cast<uint8_t>(packet[7] - '1');
    if (strncmp(packet + 9, "ON", 2) == 0) {
      handleAlarmSet(alarmIndex, AlarmAction::Enable, 0, reply, replySize);
    } else if (strncmp(packet + 9, "OFF", 3) == 0) {
      handleAlarmSet(alarmIndex, AlarmAction::Disable, 0, reply, replySize);
    } else {
      handleAlarmSet(alarmIndex, AlarmAction::SetTime, static_cast<uint16_t>(atoi(packet + 8)), reply, replySize);
    }
  } else if (startsWith(packet, "ALM_GET")) {
    handleAlarmGet(reply, replySize);
  } else if (startsWith(packet, "DAWN")) {
    handleDawn(parseUint8(packet + 4), reply, replySize);
  } else if (startsWith(packet, "DISCOVER")) {
    handleDiscover(reply, replySize);
  } else if (startsWith(packet, "TMR_GET")) {
    handleTimerGet(reply, replySize);
  } else if (startsWith(packet, "TMR_SET")) {
    const bool running = parseFixedUint8(packet + 8) != 0;
    const uint8_t option = parseFixedUint8(packet + 10);
    const uint32_t seconds = static_cast<uint32_t>(strtoul(packet + 12, nullptr, 10));
    handleTimerSet(running, option, seconds, reply, replySize);
  } else if (startsWith(packet, "FAV_GET")) {
    handleFavoritesGet(reply, replySize);
  } else if (startsWith(packet, "FAV_SET")) {
    handleFavoritesSet(packet, reply, replySize);
  } else if (startsWith(packet, "OTA")) {
    handleOta();
  } else if (startsWith(packet, "BTN")) {
    if (strncmp(packet + 4, "ON", 2) == 0) {
      handleButton(true, reply, replySize);
    } else if (strncmp(packet + 4, "OFF", 3) == 0) {
      handleButton(false, reply, replySize);
    }
  }
}

bool UpdService::startsWith(const char* packet, const char* command) const {
  return strncmp(packet, command, strlen(command)) == 0;
}

void UpdService::sendReply(const char* reply) {
  if (reply[0] == '\0') {
    return;
  }

  Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
  Udp.write(reply);
  Udp.endPacket();
}

void UpdService::writeTime(char* buffer, size_t bufferSize) const {
  snprintf(buffer, bufferSize, "%02u:%02u", _time.getHours(), _time.getMinutes());
}

void UpdService::writeCurrentState(char* reply, size_t replySize) const {
  const Effects::Id effectId = _effects.getSelectedEffectId();
  const EffectSettings& settings = _settings.getEffectSettings(effectId);
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
    _power.isOn() ? 1 : 0,
    _button.isEnabled() ? 1 : 0,
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
  if (_effects.setEffect(Effects::toId(effectId))) {
    _stateNotifier.stateChanged();
  }
  writeCurrentState(reply, replySize);
}

void UpdService::handleBrightness(uint8_t brightness, char* reply, size_t replySize) {
  _effects.setEffectBrightness(brightness);
  _stateNotifier.stateChanged();
  writeCurrentState(reply, replySize);
}

void UpdService::handleSpeed(uint8_t speed, char* reply, size_t replySize) {
  _effects.setEffectSpeed(speed);
  _stateNotifier.stateChanged();
  writeCurrentState(reply, replySize);
}

void UpdService::handleScale(uint8_t scale, char* reply, size_t replySize) {
  _effects.setEffectScale(scale);
  _stateNotifier.stateChanged();
  writeCurrentState(reply, replySize);
}

void UpdService::handlePowerOn(char* reply, size_t replySize) {
  _power.on();
  writeCurrentState(reply, replySize);
}

void UpdService::handlePowerOff(char* reply, size_t replySize) {
  _power.off();
  writeCurrentState(reply, replySize);
}

void UpdService::handleAlarmSet(uint8_t alarmIndex, UpdService::AlarmAction action, uint16_t time, char* reply, size_t replySize) {
  switch (action) {
  case AlarmAction::Enable:
    snprintf(reply, replySize, "ALM_SET%u ON", alarmIndex + 1);
    break;
  case AlarmAction::Disable:
    snprintf(reply, replySize, "ALM_SET%u OFF", alarmIndex + 1);
    break;
  case AlarmAction::SetTime:
    snprintf(reply, replySize, "ALM_SET%u %u", alarmIndex + 1, time);
    break;
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
    _port
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
  _button.setEnabled(enabled);
  _stateNotifier.stateChanged();
  writeCurrentState(reply, replySize);
}

#else

void UpdService::init() {}
void UpdService::tick() {}

#endif
