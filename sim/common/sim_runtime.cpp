#include "sim_runtime.h"
#include "sim_time.h"

#include "audio/audio_service.h"
#include "core/frame_renderer.h"
#include "core/power_controller.h"
#include "core/rotation_controller.h"
#include "core/state_notifier.h"
#include "effect/controller.h"
#include "effect/palette_catalog.h"
#include "effect/palettes.h"
#include "hardware/led.h"
#include "notification/controller.h"
#include "notification/overlay.h"
#include "storage/eeprom_store.h"
#include "storage/settings_repository.h"
#include "text/running_text.h"
#include "time/time_service.h"

#include <utility>

namespace sim {

namespace {
constexpr uint32_t SIM_TEXT_NOTIFICATION_MS = 6000;

bool isUserNotificationType(int value) {
  return value == static_cast<int>(UserNotificationType::Notify) ||
         value == static_cast<int>(UserNotificationType::Warning) ||
         value == static_cast<int>(UserNotificationType::Alarm);
}
}  // namespace

SimRuntime::SimRuntime() = default;

SimRuntime::~SimRuntime() {
  delete _frameRenderer;
  delete _rotation;
  delete _notifications;
  delete _runningText;
  delete _power;
  delete _effects;
  delete _stateNotifier;
  delete _audio;
  delete _time;
  delete _settings;
  delete _eeprom;
  delete _led;
}

bool SimRuntime::init(const RuntimeOptions& options) {
  _nowMs = 0;
  sim_millis = 0;

  _led = new Led();
  _led->init();

  _eeprom = new EepromStore();
  _settings = new SettingsRepository(*_eeprom);
  _time = new TimeService();
  _audio = new AudioService(*_eeprom);
  _stateNotifier = new StateNotifier();
  _effects = new EffectController(*_audio, *_eeprom, *_led, *_settings, *_time);
  _power = new PowerController(*_eeprom, *_effects, *_stateNotifier);
  _runningText = new RunningText(*_led, WIDTH);
  _notifications = new NotificationController(
      *_eeprom, *_power, *_runningText, *_stateNotifier, *_time);
  _rotation = new RotationController(*_eeprom, *_effects, *_stateNotifier);
  _frameRenderer = new FrameRenderer(
      *_effects, *_led, *_notifications, *_power, *_stateNotifier);

  _eeprom->init();
  // Seed power ON so the startup frame is visible instead of black.
  _eeprom->writePowerState(true);

  _settings->init();
  _time->init();
  _audio->init();
  _effects->init();
  _power->init();
  _notifications->init();
  _rotation->init();

  seedOptions(options);

  _fps.store(options.fps);
  _frameCount = 0;

  return true;
}

void SimRuntime::seedOptions(const RuntimeOptions& options) {
  if (options.effect != Effects::DEFAULT_ID && Effects::isValid(options.effect)) {
    _effects->setEffectImmediate(options.effect);
  }

  if (options.palette != Palettes::Id::Auto) {
    _effects->setPalette(options.palette);
  }

  if (options.brightness_overridden) {
    _effects->setEffectBrightness(options.brightness);
  }
  if (options.speed_overridden) {
    _effects->setEffectSpeed(options.speed);
  }
  if (options.scale_overridden) {
    _effects->setEffectScale(options.scale);
  }
}

void SimRuntime::pushCommand(const Command& cmd) {
  std::lock_guard<std::mutex> lock(_commandMutex);
  _commands.push(cmd);
}

void SimRuntime::applyCommand(const Command& cmd) {
  switch (cmd.type) {
    case CommandType::SetEffect: {
      Effects::Id id = static_cast<Effects::Id>(cmd.intValue);
      if (Effects::isValid(id)) {
        _effects->setEffect(id);
      }
      break;
    }
    case CommandType::SetPalette: {
      Palettes::Id id = Palettes::clamp(static_cast<uint8_t>(cmd.intValue));
      _effects->setPalette(id);
      break;
    }
    case CommandType::SetBrightness:
      _effects->setEffectBrightness(static_cast<uint8_t>(cmd.intValue));
      break;
    case CommandType::SetSpeed:
      _effects->setEffectSpeed(static_cast<uint8_t>(cmd.intValue));
      break;
    case CommandType::SetScale:
      _effects->setEffectScale(static_cast<uint8_t>(cmd.intValue));
      break;
    case CommandType::ResetEffectSettings:
      _effects->resetEffectSettingsToDefaults();
      break;
    case CommandType::SetFps:
      if (cmd.intValue > 0) {
        _fps.store(static_cast<uint8_t>(cmd.intValue));
      }
      break;
    case CommandType::Pause:
      _paused.store(cmd.intValue != 0);
      break;
    case CommandType::Step:
      _step.store(true);
      break;
    case CommandType::PowerOn:
      _power->on();
      break;
    case CommandType::PowerOff:
      _power->off();
      break;
    case CommandType::PowerToggle:
      _power->toggle();
      break;
    case CommandType::NextEffect:
      _effects->setNextEffect();
      break;
    case CommandType::PrevEffect:
      _effects->setPreviousEffect();
      break;
    case CommandType::NotifyText:
      _notifications->startUserTextNotification(
          String(cmd.strValue.c_str()), CRGB::White, SIM_TEXT_NOTIFICATION_MS);
      break;
    case CommandType::NotifyUser:
      if (isUserNotificationType(cmd.intValue)) {
        _notifications->startUserNotification(
            static_cast<UserNotificationType>(cmd.intValue), 0);
      }
      break;
    case CommandType::StopNotification:
      _notifications->stopUserNotification();
      break;
    case CommandType::ButtonPress:
      if (cmd.intValue >= 1 && cmd.intValue <= 5) {
        buttonPress(static_cast<uint8_t>(cmd.intValue));
      }
      break;
    case CommandType::ButtonRelease:
      buttonRelease();
      break;
    case CommandType::ButtonTap:
      if (cmd.intValue >= 1 && cmd.intValue <= 5) {
        buttonTap(static_cast<uint8_t>(cmd.intValue));
      }
      break;
    case CommandType::ButtonHold:
      buttonHold();
      break;
    case CommandType::ButtonStep:
      buttonStep();
      break;
    case CommandType::SetAudioMode:
      if (cmd.intValue >= 0 && cmd.intValue <= static_cast<int>(AudioMode::Effect)) {
        _audio->setMode(static_cast<AudioMode>(cmd.intValue));
      }
      break;
    case CommandType::SetAudioBand:
      if (cmd.intValue >= 0 && cmd.intValue <= static_cast<int>(AudioBand::Treble)) {
        _audio->setBand(static_cast<AudioBand>(cmd.intValue));
      }
      break;
    case CommandType::SetAudioAmount:
      if (cmd.intValue >= 0 && cmd.intValue <= 255) {
        _audio->setAmount(static_cast<uint8_t>(cmd.intValue));
      }
      break;
    case CommandType::Quit:
      _quit.store(true);
      break;
    default:
      break;
  }
}

void SimRuntime::buttonPress(uint8_t count) {
  if (count == 0 || count > 5) return;
  _notifications->onButtonPress(count);
}

void SimRuntime::buttonRelease() {
  _notifications->onButtonRelease();
}

void SimRuntime::buttonTap(uint8_t count) {
  if (count == 0 || count > 5) return;
  if (count == 1) {
    if (_notifications->isUserNotificationActive()) {
      _notifications->stopUserNotification();
      _notifications->onButtonDismiss();
    } else {
      const bool wasOn = _power->isOn();
      _rotation->disable();
      _power->toggle();
      if (!wasOn && _power->isOn()) {
        _notifications->onButtonPowerOn();
      } else {
        _notifications->onButtonPowerOff();
      }
    }
    return;
  }
  // 5-tap IP behavior skipped in Phase 4.
  if (count == 5) return;
  if (!_power->isOn()) return;
  if (count == 2) {
    _rotation->disable();
    _effects->setNextEffect();
    _notifications->onButtonNextEffect();
    _stateNotifier->stateChanged();
  } else if (count == 3) {
    _rotation->disable();
    _effects->setPreviousEffect();
    _notifications->onButtonPreviousEffect();
    _stateNotifier->stateChanged();
  }
}

void SimRuntime::buttonHold() {
  if (!_power->isOn()) return;
  _buttonBrightDirection = !_buttonBrightDirection;
  const EffectSettings& s = _settings->getEffectSettings(_effects->getSelectedEffectId());
  _notifications->onButtonBrightness(s.brightness, _buttonBrightDirection);
}

void SimRuntime::buttonStep() {
  if (!_power->isOn()) return;
  const EffectSettings& effectSettings = _settings->getEffectSettings(_effects->getSelectedEffectId());
  uint8_t newBrightness = effectSettings.brightness;
  if (_buttonBrightDirection) {
    if (effectSettings.brightness < 10U) newBrightness = effectSettings.brightness + 1U;
    else if (effectSettings.brightness < 250U) newBrightness = effectSettings.brightness + 5U;
    else newBrightness = 255U;
  } else {
    if (effectSettings.brightness > 15U) newBrightness = effectSettings.brightness - 5U;
    else if (effectSettings.brightness > 1U) newBrightness = effectSettings.brightness - 1U;
    else newBrightness = 1U;
  }
  _effects->setEffectBrightness(newBrightness);
  _notifications->onButtonBrightness(newBrightness, _buttonBrightDirection);
  _stateNotifier->stateChanged();
}

bool SimRuntime::tick(uint32_t now_ms) {
  _nowMs = now_ms;
  sim_millis = _nowMs;

  // 1. Consume queued commands before touching controllers.
  std::queue<Command> pending;
  {
    std::lock_guard<std::mutex> lock(_commandMutex);
    pending.swap(_commands);
  }
  while (!pending.empty()) {
    applyCommand(pending.front());
    pending.pop();
  }

  // 2. Optional rotation.
  _rotation->tick();

  // 3. Power fade state.
  _power->tick();

  // 4. Audio (no-op in sim unless ADC enabled).
  _audio->tick();

  // 5. Render through the production visual pipeline.
  const bool renderFrame = !_paused.load() || _step.exchange(false);
  if (renderFrame) {
    _frameRenderer->renderNow();
    ++_frameCount;
  }

  // 6. Persist settings after the normal firmware delay.
  _settings->tick(_effects->getActiveEffectId());

  // 7. Advance host-emulated time.
  _time->tick();

  return renderFrame;
}

Effects::Id SimRuntime::activeEffect() const {
  return _effects ? _effects->getActiveEffectId() : Effects::DEFAULT_ID;
}

uint8_t SimRuntime::outputBrightness() const {
  return _effects ? _effects->getOutputBrightness() : 255;
}

AudioConfig SimRuntime::audioConfig() const {
  return _audio ? _audio->config() : AudioConfig{};
}

AudioFrame SimRuntime::audioFrame() const {
  return _audio ? _audio->frame() : AudioFrame{};
}

void SimRuntime::copyFrameRgb(uint8_t* dst) const {
  if (!_led) return;
  for (uint16_t row = 0; row < HEIGHT; ++row) {
    const uint16_t y = HEIGHT - row - 1;
    for (uint16_t x = 0; x < WIDTH; ++x) {
      const CRGB& c = _led->getPixel(static_cast<uint8_t>(x), static_cast<uint8_t>(y));
      *dst++ = c.r;
      *dst++ = c.g;
      *dst++ = c.b;
    }
  }
}

uint8_t SimRuntime::effectCount() { return Effects::DISPLAY_COUNT; }

Effects::Id SimRuntime::effectIdAt(uint8_t index) {
  if (index >= Effects::DISPLAY_COUNT) return Effects::Id::INVALID;
  return Effects::DISPLAY_ORDER[index];
}

const char* SimRuntime::effectName(Effects::Id id) {
  return Effects::getEffectName(id);
}

EffectSettingsSpec SimRuntime::effectSettingsSpec(Effects::Id id) {
  return Effects::getEffectSettingsSpec(id);
}

uint8_t SimRuntime::paletteCount() { return Palettes::COUNT; }

Palettes::Id SimRuntime::paletteIdAt(uint8_t index) {
  if (index == 0) return Palettes::Id::Auto;
  if (index - 1 >= Palettes::SELECTABLE_COUNT) return Palettes::Id::Auto;
  return Palettes::SELECTABLE_ORDER[index - 1];
}

const char* SimRuntime::paletteName(Palettes::Id id) {
  return Palettes::getPaletteName(id);
}

}  // namespace sim
