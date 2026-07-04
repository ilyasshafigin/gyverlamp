#include "sim_audio_input.h"
#include "sim_runtime.h"
#include "sim_time.h"

#include "effect/catalog.h"
#include "effect/palette_catalog.h"
#include "effect/palette_ids.h"
#include "effect/settings.h"
#include "notification/types.h"
#include "util/fft.h"

#include <cstdint>
#include <cstring>
#include <new>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define WASM_KEEPALIVE EMSCRIPTEN_KEEPALIVE
#else
#define WASM_KEEPALIVE
#endif

namespace {

  sim::SimRuntime* g_runtime = nullptr;
  uint8_t* g_framebuffer = nullptr;
  bool g_initialized = false;
  uint32_t g_nowMs = 0;
  constexpr size_t FRAMEBUFFER_SIZE = static_cast<size_t>(WIDTH) * HEIGHT * 3;
  constexpr size_t MAX_NOTIFY_TEXT_LEN = 160;

  void ensureFramebuffer() {
    if (!g_framebuffer) {
      g_framebuffer = new uint8_t[FRAMEBUFFER_SIZE]();
    }
  }

  void ensureRuntime() {
    if (!g_runtime) {
      g_runtime = new sim::SimRuntime();
    }
  }

  sim::RuntimeOptions defaultOptions() {
    sim::RuntimeOptions options;
    options.effect = Effects::DEFAULT_ID;
    options.fps = 30;
    options.brightness = 255;
    options.speed = 128;
    options.scale = 128;
    options.palette = Palettes::Id::Auto;
    options.brightness_overridden = false;
    options.speed_overridden = false;
    options.scale_overridden = false;
    return options;
  }

  void lazyInit() {
    ensureRuntime();
    ensureFramebuffer();
    if (!g_initialized) {
      std::memset(g_framebuffer, 0, FRAMEBUFFER_SIZE);
      g_runtime->init(defaultOptions());
      g_initialized = true;
    }
  }

} // namespace

extern "C" {

WASM_KEEPALIVE int sim_init() {
  ensureRuntime();
  ensureFramebuffer();
  std::memset(g_framebuffer, 0, FRAMEBUFFER_SIZE);

  if (!g_runtime->init(defaultOptions())) {
    g_initialized = false;
    return 0;
  }
  g_initialized = true;
  return 1;
}

WASM_KEEPALIVE int sim_tick(double now_ms) {
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;

  g_nowMs = static_cast<uint32_t>(now_ms);
  if (g_runtime->tick(g_nowMs)) {
    g_runtime->copyFrameRgb(g_framebuffer);
  }
  return 1;
}

WASM_KEEPALIVE int sim_update(double now_ms) {
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;

  g_nowMs = static_cast<uint32_t>(now_ms);
  // SimRuntime has no separate update path; tick advances controllers and
  // renders a frame.
  g_runtime->tick(g_nowMs);
  return 1;
}

WASM_KEEPALIVE int sim_render() {
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;

  // Render current state into the framebuffer. Use the last known time so that
  // paused/step behavior remains consistent.
  g_runtime->tick(g_nowMs);
  g_runtime->copyFrameRgb(g_framebuffer);
  return 1;
}

WASM_KEEPALIVE const uint8_t* sim_framebuffer() {
  ensureFramebuffer();
  return g_framebuffer;
}

WASM_KEEPALIVE int sim_width() {
  return WIDTH;
}
WASM_KEEPALIVE int sim_height() {
  return HEIGHT;
}

WASM_KEEPALIVE int sim_set_effect(int id) {
  if (id < 0 || id > 255) return 0;
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  Effects::Id eid = Effects::toId(static_cast<uint8_t>(id));
  if (!Effects::isValid(eid)) return 0;
  g_runtime->pushCommand({sim::CommandType::SetEffect, static_cast<int>(eid)});
  return 1;
}

WASM_KEEPALIVE int sim_set_palette(int id) {
  if (id < 0 || id > 255) return 0;
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  Palettes::Id pid = static_cast<Palettes::Id>(id);
  if (!Palettes::isValid(Palettes::toIndex(pid))) return 0;
  g_runtime->pushCommand({sim::CommandType::SetPalette, id});
  return 1;
}

WASM_KEEPALIVE int sim_set_brightness(int v) {
  if (v < 0 || v > 255) return 0;
  lazyInit();
  if (g_runtime && g_initialized) {
    g_runtime->pushCommand({sim::CommandType::SetBrightness, v});
  }
  return 1;
}

WASM_KEEPALIVE int sim_set_speed(int v) {
  if (v < 0 || v > 255) return 0;
  lazyInit();
  if (g_runtime && g_initialized) {
    g_runtime->pushCommand({sim::CommandType::SetSpeed, v});
  }
  return 1;
}

WASM_KEEPALIVE int sim_set_scale(int v) {
  if (v < 0 || v > 255) return 0;
  lazyInit();
  if (g_runtime && g_initialized) {
    g_runtime->pushCommand({sim::CommandType::SetScale, v});
  }
  return 1;
}

WASM_KEEPALIVE int sim_reset_defaults() {
  lazyInit();
  if (g_runtime && g_initialized) {
    g_runtime->pushCommand({sim::CommandType::ResetEffectSettings, 0});
  }
  return 1;
}

WASM_KEEPALIVE int sim_power(int mode) {
  if (mode < 0 || mode > 2) return 0;
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  sim::CommandType cmd = sim::CommandType::PowerToggle;
  if (mode == 0) cmd = sim::CommandType::PowerOff;
  if (mode == 1) cmd = sim::CommandType::PowerOn;
  g_runtime->pushCommand({cmd, 0});
  return 1;
}

WASM_KEEPALIVE int sim_effect_next() {
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::NextEffect, 0});
  return 1;
}

WASM_KEEPALIVE int sim_effect_prev() {
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::PrevEffect, 0});
  return 1;
}

WASM_KEEPALIVE int sim_notify_text(const char* message) {
  if (!message) return 0;
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  std::string text(message);
  if (text.empty()) return 0;
  if (text.length() > MAX_NOTIFY_TEXT_LEN) text.resize(MAX_NOTIFY_TEXT_LEN);
  g_runtime->pushCommand({sim::CommandType::NotifyText, 0, text});
  return 1;
}

WASM_KEEPALIVE int sim_notify_user(int type) {
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  if (
    type != static_cast<int>(UserNotificationType::Notify) && type != static_cast<int>(UserNotificationType::Warning) &&
    type != static_cast<int>(UserNotificationType::Alarm)
  ) {
    return 0;
  }
  g_runtime->pushCommand({sim::CommandType::NotifyUser, type});
  return 1;
}

WASM_KEEPALIVE int sim_notify_clear() {
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::StopNotification, 0});
  return 1;
}

WASM_KEEPALIVE int sim_button_press(int count) {
  if (count < 1 || count > 5) return 0;
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::ButtonPress, count});
  return 1;
}

WASM_KEEPALIVE int sim_button_release() {
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::ButtonRelease, 0});
  return 1;
}

WASM_KEEPALIVE int sim_button_tap(int count) {
  if (count < 1 || count > 5) return 0;
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::ButtonTap, count});
  return 1;
}

WASM_KEEPALIVE int sim_button_hold() {
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::ButtonHold, 0});
  return 1;
}

WASM_KEEPALIVE int sim_button_step() {
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::ButtonStep, 0});
  return 1;
}

WASM_KEEPALIVE int sim_audio_push_sample(int sample) {
  if (!sim::audioEnabled()) return 0;
  if (sample < 0) sample = 0;
  if (sample > 1023) sample = 1023;
  sim::audioPushSample(static_cast<uint16_t>(sample));
  return 1;
}

WASM_KEEPALIVE void sim_audio_set_enabled(int enabled) {
  sim::audioSetEnabled(enabled != 0);
}

WASM_KEEPALIVE void sim_audio_flush() {
  sim::audioFlush();
}

WASM_KEEPALIVE int sim_audio_fft_size() {
  return FFT_SIZE;
}

WASM_KEEPALIVE int sim_set_audio_mode(int mode) {
  if (mode < 0 || mode > static_cast<int>(AudioMode::Effect)) return 0;
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::SetAudioMode, mode});
  return 1;
}

WASM_KEEPALIVE int sim_set_audio_band(int band) {
  if (band < 0 || band > static_cast<int>(AudioBand::Treble)) return 0;
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::SetAudioBand, band});
  return 1;
}

WASM_KEEPALIVE int sim_set_audio_amount(int amount) {
  if (amount < 0 || amount > 255) return 0;
  lazyInit();
  if (!g_runtime || !g_initialized) return 0;
  g_runtime->pushCommand({sim::CommandType::SetAudioAmount, amount});
  return 1;
}

WASM_KEEPALIVE int sim_audio_mode() {
  lazyInit();
  return g_runtime ? static_cast<int>(g_runtime->audioConfig().mode) : 0;
}

WASM_KEEPALIVE int sim_audio_band() {
  lazyInit();
  return g_runtime ? static_cast<int>(g_runtime->audioConfig().band) : 0;
}

WASM_KEEPALIVE int sim_audio_amount() {
  lazyInit();
  return g_runtime ? g_runtime->audioConfig().amount : 128;
}

WASM_KEEPALIVE int sim_audio_level() {
  lazyInit();
  return g_runtime ? g_runtime->audioFrame().level : 0;
}

WASM_KEEPALIVE int sim_audio_bass() {
  lazyInit();
  return g_runtime ? g_runtime->audioFrame().bass : 0;
}

WASM_KEEPALIVE int sim_audio_treble() {
  lazyInit();
  return g_runtime ? g_runtime->audioFrame().treble : 0;
}

WASM_KEEPALIVE int sim_audio_available() {
  lazyInit();
  return g_runtime && g_runtime->audioFrame().available ? 1 : 0;
}

WASM_KEEPALIVE uint32_t sim_audio_underruns() {
  return sim::audioUnderrunCount();
}

WASM_KEEPALIVE uint32_t sim_audio_overflows() {
  return sim::audioOverflowCount();
}

WASM_KEEPALIVE uint8_t sim_effect_count() {
  return sim::SimRuntime::effectCount();
}

WASM_KEEPALIVE int sim_effect_id_at(int index) {
  if (index < 0 || index >= sim::SimRuntime::effectCount()) return -1;
  return Effects::toIndex(sim::SimRuntime::effectIdAt(index));
}

WASM_KEEPALIVE const char* sim_effect_name_at(int index) {
  if (index < 0 || index >= sim::SimRuntime::effectCount()) return "";
  Effects::Id id = sim::SimRuntime::effectIdAt(index);
  if (!Effects::isValid(id)) return "";
  return sim::SimRuntime::effectName(id);
}

WASM_KEEPALIVE uint8_t sim_effect_default_brightness(int index) {
  if (index < 0 || index >= sim::SimRuntime::effectCount()) return 0;
  Effects::Id id = sim::SimRuntime::effectIdAt(index);
  if (!Effects::isValid(id)) return 0;
  return sim::SimRuntime::effectSettingsSpec(id).defaultBrightness;
}

WASM_KEEPALIVE uint8_t sim_effect_default_speed(int index) {
  if (index < 0 || index >= sim::SimRuntime::effectCount()) return 0;
  Effects::Id id = sim::SimRuntime::effectIdAt(index);
  if (!Effects::isValid(id)) return 0;
  return sim::SimRuntime::effectSettingsSpec(id).defaultSpeed;
}

WASM_KEEPALIVE uint8_t sim_effect_default_scale(int index) {
  if (index < 0 || index >= sim::SimRuntime::effectCount()) return 0;
  Effects::Id id = sim::SimRuntime::effectIdAt(index);
  if (!Effects::isValid(id)) return 0;
  return sim::SimRuntime::effectSettingsSpec(id).defaultScale;
}

WASM_KEEPALIVE uint8_t sim_effect_reset_on_change(int index) {
  if (index < 0 || index >= sim::SimRuntime::effectCount()) return 0;
  Effects::Id id = sim::SimRuntime::effectIdAt(index);
  if (!Effects::isValid(id)) return 0;
  return sim::SimRuntime::effectSettingsSpec(id).resetOnChange;
}

WASM_KEEPALIVE uint8_t sim_palette_count() {
  return sim::SimRuntime::paletteCount();
}

WASM_KEEPALIVE int sim_palette_id_at(int index) {
  if (index < 0 || index >= sim::SimRuntime::paletteCount()) return -1;
  return Palettes::toIndex(sim::SimRuntime::paletteIdAt(index));
}

WASM_KEEPALIVE const char* sim_palette_name_at(int index) {
  if (index < 0 || index >= sim::SimRuntime::paletteCount()) return "";
  Palettes::Id id = sim::SimRuntime::paletteIdAt(index);
  if (!Palettes::isValid(Palettes::toIndex(id))) return "";
  return sim::SimRuntime::paletteName(id);
}

} // extern "C"
