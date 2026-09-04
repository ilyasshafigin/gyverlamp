#include "lamp.h"

void Lamp::setup() {
  led.init();
  button.detect();

  Serial.println();
  delay(1000);

  eeprom.init();
  connectivity.load();
  settings.init();
  audio.init();
  notifications.init();
  effects.init();
  power.init();
  button.init();

  wifi.setConnectingHandler([this] { notifications.onWifiConnecting(); });
  wifi.setConnectedHandler([this] { notifications.onWifiConnected(); });
  wifi.setErrorHandler([this] { notifications.onWifiError(); });
  wifi.setDisabledHandler([this] { notifications.onWifiDisabled(); });

  ota.setStartHandler([this] {
    notifications.onOtaStart();
    frameRenderer.renderNow();
  });

  ota.setProgressHandler([this](uint8_t percent) {
    notifications.onOtaProgress(percent);
    frameRenderer.render();
  });

  ota.setEndHandler([this] {
    notifications.onOtaEnd();
    frameRenderer.renderNow();
  });

  ota.setErrorHandler([this]() {
    notifications.onOtaError();
    frameRenderer.renderNow();
  });

  wifi.init(connectivity.wifiConfig());
  upd.init();
  ota.init(wifi.getDeviceId().c_str(), connectivity.otaEnabled());
  mqtt.init(connectivity.mqttConfig());
  time.init();
  web.init();
  rotation.init();
}

void Lamp::loop() {
  power.tick();

  LoopProfiler::measure(LoopProfiler::ROTATION, [this]() { rotation.tick(power.isOn()); });

  const bool audioReadEnabled = power.isOn() && audio.config().mode != AudioMode::Off;
  LoopProfiler::measure(LoopProfiler::AUDIO, [this, audioReadEnabled]() { audio.tick(audioReadEnabled); });
  LoopProfiler::measure(LoopProfiler::RENDER, [this]() { frameRenderer.render(); });
  LoopProfiler::measure(LoopProfiler::SETTINGS, [this]() { settings.tick(effects.getActiveEffectId()); });
  LoopProfiler::measure(LoopProfiler::TIME, [this]() { time.tick(); });
  LoopProfiler::measure(LoopProfiler::BUTTON, [this]() { button.tick(); });
  LoopProfiler::measure(LoopProfiler::WIFI, [this]() { wifi.tick(); });
  LoopProfiler::measure(LoopProfiler::OTA, [this]() { ota.tick(wifi.isStaConnected()); });
  LoopProfiler::measure(LoopProfiler::UDP, [this]() { upd.tick(); });
  LoopProfiler::measure(LoopProfiler::WEB, [this]() { web.tick(); });
  LoopProfiler::measure(LoopProfiler::MQTT, [this]() { mqtt.tick(); });

  if (stateNotifier.consumeChanged()) {
    mqtt.updateStates();
  }

  LoopProfiler::tick();
  yield();
}
