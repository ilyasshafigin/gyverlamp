#include "lamp.h"

void Lamp::setup() {
  led.init();
  button.detect();

  Serial.println();
  delay(1000);

  if (!eeprom.init()) {
    led.blackout();
    safeMode_ = true;
    Serial.println(F("[SAFE MODE] EEPROM initialization failed; settings services disabled"));
    return;
  }
  connectivity.load();
  settings.init();
  audio.init();
  notifications.init();
  effects.init();
  power.init();
  button.init();

  wifi.begin(connectivity.wifiRuntimeConfig(), onWifiEvent, this);
  upd.init();
  const WifiController::Snapshot wifiSnapshot = wifi.snapshot();
  const OtaController::Config otaConfig{wifiSnapshot.deviceId, 8266, connectivity.otaEnabled(), nullptr, nullptr};
  ota.begin(otaConfig, onOtaEvent, this);
  mqtt.init(connectivity.mqttConfig());
  time.init();
  web.init();
  rotation.init();
}

void Lamp::loop() {
  if (safeMode_) {
    yield();
    return;
  }

  power.tick();

  LoopProfiler::measure(LoopProfiler::ROTATION, [this]() { rotation.tick(power.isOn()); });

  const bool audioReadEnabled = power.isOn() && audio.config().mode != AudioMode::Off;
  LoopProfiler::measure(LoopProfiler::AUDIO, [this, audioReadEnabled]() { audio.tick(audioReadEnabled); });
  LoopProfiler::measure(LoopProfiler::RENDER, [this]() { frameRenderer.render(); });
  LoopProfiler::measure(LoopProfiler::SETTINGS, [this]() { settings.tick(effects.getActiveEffectId()); });
  LoopProfiler::measure(LoopProfiler::TIME, [this]() { time.tick(); });
  LoopProfiler::measure(LoopProfiler::BUTTON, [this]() { button.tick(); });
  LoopProfiler::measure(LoopProfiler::WIFI, [this]() { wifi.tick(); });
  LoopProfiler::measure(LoopProfiler::OTA, [this]() { ota.tick(wifi.staConnected()); });
  LoopProfiler::measure(LoopProfiler::UDP, [this]() { upd.tick(); });
  LoopProfiler::measure(LoopProfiler::WEB, [this]() { web.tick(); });
  LoopProfiler::measure(LoopProfiler::MQTT, [this]() { mqtt.tick(); });

  if (stateNotifier.consumeChanged()) {
    mqtt.updateStates();
  }

  LoopProfiler::tick();
  yield();
}

void Lamp::onWifiEvent(const WifiController::Event& event, void* context) {
  auto* lamp = static_cast<Lamp*>(context);
  switch (event.type) {
    case WifiController::EventType::Connecting: lamp->notifications.onWifiConnecting(); break;
    case WifiController::EventType::Connected: lamp->notifications.onWifiConnected(); break;
    case WifiController::EventType::Error: lamp->notifications.onWifiError(); break;
    case WifiController::EventType::Disabled: lamp->notifications.onWifiDisabled(); break;
  }
}

void Lamp::onOtaEvent(const OtaController::Event& event, void* context) {
  auto* lamp = static_cast<Lamp*>(context);
  switch (event.type) {
    case OtaController::EventType::Start:
      lamp->notifications.onOtaStart();
      lamp->frameRenderer.renderNow();
      break;
    case OtaController::EventType::Progress:
      lamp->notifications.onOtaProgress(event.progress);
      lamp->frameRenderer.render();
      break;
    case OtaController::EventType::End:
      lamp->notifications.onOtaEnd();
      lamp->frameRenderer.renderNow();
      break;
    case OtaController::EventType::Error:
      lamp->notifications.onOtaError();
      lamp->frameRenderer.renderNow();
      break;
  }
}
