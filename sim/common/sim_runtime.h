#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>

#include "effect/catalog.h"
#include "effect/palette_ids.h"
#include "audio/audio_config.h"
#include "audio/audio_frame.h"

class AudioService;
class EepromStore;
class EffectController;
class FrameRenderer;
class Led;
class NotificationController;
class PowerController;
class RotationController;
class RunningText;
class SettingsRepository;
class StateNotifier;
class TimeService;

namespace sim {

struct RuntimeOptions {
  Effects::Id effect = Effects::DEFAULT_ID;
  uint8_t fps = 30;
  uint8_t brightness = 255;
  uint8_t speed = 128;
  uint8_t scale = 128;
  Palettes::Id palette = Palettes::Id::Auto;
  bool brightness_overridden = false;
  bool speed_overridden = false;
  bool scale_overridden = false;
};

enum class CommandType : uint8_t {
  None,
  SetEffect,
  SetPalette,
  SetBrightness,
  SetSpeed,
  SetScale,
  ResetEffectSettings,
  SetFps,
  Pause,
  Step,
  PowerOn,
  PowerOff,
  PowerToggle,
  NextEffect,
  PrevEffect,
  NotifyText,
  NotifyUser,
  StopNotification,
  ButtonPress,
  ButtonRelease,
  ButtonTap,
  ButtonHold,
  ButtonStep,
  SetAudioMode,
  SetAudioBand,
  SetAudioAmount,
  Quit,
};

struct Command {
  CommandType type = CommandType::None;
  int intValue = 0;
  std::string strValue;
};

// Thin facade that owns the production visual graph and drives it with a safe
// command queue. CLI/Web/WASM protocol mapping stays outside.
//
// Runtime order (documented in code):
//   1. consume queued commands
//   2. optional rotation tick
//   3. power.tick
//   4. audio.tick
//   5. frameRenderer.render/renderNow
//   6. settings.tick
//   7. time.tick
class SimRuntime {
 public:
  SimRuntime();
  ~SimRuntime();

  SimRuntime(const SimRuntime&) = delete;
  SimRuntime& operator=(const SimRuntime&) = delete;

  bool init(const RuntimeOptions& options);

  // Advance the runtime by one loop iteration. Returns true when a frame was
  // emitted. Safe to call from a single runtime thread only.
  bool tick(uint32_t now_ms);

  // Thread-safe command enqueue. The runtime thread applies commands before
  // any controller ticks or rendering.
  void pushCommand(const Command& cmd);

  bool shouldQuit() const { return _quit.load(); }
  uint8_t fps() const { return _fps.load(); }
  bool paused() const { return _paused.load(); }

  Effects::Id activeEffect() const;
  uint32_t frameCount() const { return _frameCount; }
  uint32_t nowMs() const { return _nowMs; }
  uint8_t outputBrightness() const;
  AudioConfig audioConfig() const;
  AudioFrame audioFrame() const;

  // Copy the rendered frame as visual row-major RGB (bottom row first).
  void copyFrameRgb(uint8_t* dst) const;

  // Static catalog helpers used by the WASM runner to build the web UI.
  static uint8_t effectCount();
  static Effects::Id effectIdAt(uint8_t index);
  static const char* effectName(Effects::Id id);
  static EffectSettingsSpec effectSettingsSpec(Effects::Id id);

  static uint8_t paletteCount();
  static Palettes::Id paletteIdAt(uint8_t index);
  static const char* paletteName(Palettes::Id id);

 private:
  void applyCommand(const Command& cmd);
  void seedOptions(const RuntimeOptions& options);

  // Button simulation helpers. These mirror production TouchButton semantics
  // without hardware/EncButton. They are invoked from applyCommand().
  void buttonPress(uint8_t count);
  void buttonRelease();
  void buttonTap(uint8_t count);
  void buttonHold();
  void buttonStep();

  std::mutex _commandMutex;
  std::queue<Command> _commands;

  Led* _led = nullptr;
  EepromStore* _eeprom = nullptr;
  SettingsRepository* _settings = nullptr;
  TimeService* _time = nullptr;
  AudioService* _audio = nullptr;
  StateNotifier* _stateNotifier = nullptr;
  EffectController* _effects = nullptr;
  PowerController* _power = nullptr;
  RunningText* _runningText = nullptr;
  NotificationController* _notifications = nullptr;
  RotationController* _rotation = nullptr;
  FrameRenderer* _frameRenderer = nullptr;

  uint32_t _nowMs = 0;
  uint32_t _frameCount = 0;

  std::atomic<uint8_t> _fps{30};
  std::atomic<bool> _paused{false};
  std::atomic<bool> _step{false};
  std::atomic<bool> _quit{false};
  bool _buttonBrightDirection = false;
};

}  // namespace sim
