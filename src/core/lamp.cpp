#include "lamp.h"

void Lamp::setup() {
  led.init();
  button.detect();

  Serial.println();
  delay(1000);

  eeprom.init();
  settings.init();
  audio.init();
  notifications.init();
  effects.init();
  power.init();
  button.init();

  wifi.init();
  upd.init();
  ota.init();
  mqtt.init();
  time.init();
  web.init();
  rotation.init();
}

void Lamp::loop() {
  LoopProfiler::measure(LoopProfiler::ROTATION, [this]() {
    rotation.tick();
    });

  power.tick();

  LoopProfiler::measure(LoopProfiler::AUDIO, [this]() {
    audio.tick();
    });

  LoopProfiler::measure(LoopProfiler::RENDER, [this]() {
    frameRenderer.render();
    });

  LoopProfiler::measure(LoopProfiler::SETTINGS, [this]() {
    settings.tick(effects.getActiveEffectId());
    });

  LoopProfiler::measure(LoopProfiler::TIME, [this]() {
    time.tick();
    });

  LoopProfiler::measure(LoopProfiler::BUTTON, [this]() {
    button.tick();
    });

  LoopProfiler::measure(LoopProfiler::WIFI, [this]() {
    wifi.tick();
    });

  LoopProfiler::measure(LoopProfiler::OTA, [this]() {
    ota.tick();
    });

  LoopProfiler::measure(LoopProfiler::UDP, [this]() {
    upd.tick();
    });

  LoopProfiler::measure(LoopProfiler::WEB, [this]() {
    web.tick();
    });

  LoopProfiler::measure(LoopProfiler::MQTT, [this]() {
    mqtt.tick();
    });

  if (stateNotifier.consumeChanged()) {
    mqtt.updateStates();
  }

  LoopProfiler::tick();
  yield();
}
