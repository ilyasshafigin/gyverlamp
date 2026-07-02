#pragma once

#include <Arduino.h>
#ifdef USE_UDP
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#endif

class EffectController;
class PowerController;
class SettingsRepository;
class StateNotifier;
class TimeService;
class TouchButton;

class UpdService {
public:
  explicit UpdService(
    EffectController& effects,
    PowerController& power,
    SettingsRepository& settings,
    StateNotifier& stateNotifier,
    TimeService& time,
    TouchButton& button,
    uint16_t port
  )
#ifdef USE_UDP
    : _effects(effects),
    _power(power),
    _settings(settings),
    _stateNotifier(stateNotifier),
    _time(time),
    _button(button),
    _port(port)
#endif
  {
#ifndef USE_UDP
    (void)effects;
    (void)power;
    (void)settings;
    (void)stateNotifier;
    (void)time;
    (void)button;
    (void)port;
#endif
  }

  void init();
  void tick();

private:
#ifdef USE_UDP
  enum class AlarmAction : uint8_t {
    SetTime,
    Enable,
    Disable,
  };

  static constexpr size_t PACKET_BUFFER_SIZE = 255;
  static constexpr size_t REPLY_BUFFER_SIZE = 255;

  EffectController& _effects;
  PowerController& _power;
  SettingsRepository& _settings;
  StateNotifier& _stateNotifier;
  TimeService& _time;
  TouchButton& _button;
  WiFiUDP Udp;
  uint16_t _port;

  void handlePacket(const char* packet, int length, char* reply, size_t replySize);
  bool startsWith(const char* packet, const char* command) const;
  void sendReply(const char* reply);
  void writeTime(char* buffer, size_t bufferSize) const;
  void writeCurrentState(char* reply, size_t replySize) const;

  void handleDebug(char* reply, size_t replySize);
  void handleGet(char* reply, size_t replySize);
  void handleEffect(uint8_t effectId, char* reply, size_t replySize);
  void handleBrightness(uint8_t brightness, char* reply, size_t replySize);
  void handleSpeed(uint8_t speed, char* reply, size_t replySize);
  void handleScale(uint8_t scale, char* reply, size_t replySize);
  void handlePowerOn(char* reply, size_t replySize);
  void handlePowerOff(char* reply, size_t replySize);
  void handleAlarmSet(uint8_t alarmIndex, AlarmAction action, uint16_t time, char* reply, size_t replySize);
  void handleAlarmGet(char* reply, size_t replySize);
  void handleDawn(uint8_t dawnMode, char* reply, size_t replySize);
  void handleDiscover(char* reply, size_t replySize);
  void handleTimerGet(char* reply, size_t replySize);
  void handleTimerSet(bool running, uint8_t option, uint32_t seconds, char* reply, size_t replySize);
  void handleFavoritesGet(char* reply, size_t replySize);
  void handleFavoritesSet(const char* packet, char* reply, size_t replySize);
  void handleOta();
  void handleButton(bool enabled, char* reply, size_t replySize);
#endif
};
