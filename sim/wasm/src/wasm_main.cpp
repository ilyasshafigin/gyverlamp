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
  bool g_jobInitialized = false;
  uint32_t g_nowMs = 0;
  uint32_t g_nextJobFrameMs = 0;
  constexpr uint32_t JOB_FRAME_MS = FRAME_MS;
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
    options.effect = Effects::kDefaultId;
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

WASM_KEEPALIVE int sim_job_init(
  int effect_id, int palette_id, int brightness, int speed, int scale, uint32_t seed, double clock_start_utc_ms
) {
  // Export jobs deliberately get one fresh module/runtime. Browser controls use
  // sim_init and retain their legacy lifecycle.
  if (
    g_jobInitialized || g_initialized || effect_id < 0 || effect_id > 255 || palette_id < 0 || palette_id > 255 ||
    brightness < 0 || brightness > 255 || speed < 0 || speed > 255 || scale < 0 || scale > 255 ||
    clock_start_utc_ms < 0 || clock_start_utc_ms > 9007199254740991.0 ||
    clock_start_utc_ms != static_cast<double>(static_cast<uint64_t>(clock_start_utc_ms))
  ) {
    return 0;
  }

  const Effects::Id effect = Effects::toId(static_cast<uint8_t>(effect_id));
  const Palettes::Id palette = static_cast<Palettes::Id>(palette_id);
  if (!Effects::isValid(effect) || !Palettes::isValid(Palettes::toIndex(palette))) return 0;

  ensureRuntime();
  ensureFramebuffer();
  std::memset(g_framebuffer, 0, FRAMEBUFFER_SIZE);

  sim::RuntimeOptions options = defaultOptions();
  options.effect = effect;
  options.palette = palette;
  options.brightness = static_cast<uint8_t>(brightness);
  options.speed = static_cast<uint8_t>(speed);
  options.scale = static_cast<uint8_t>(scale);
  options.brightness_overridden = true;
  options.speed_overridden = true;
  options.scale_overridden = true;
  options.seed = seed;
  options.clock_start_utc_ms = static_cast<uint64_t>(clock_start_utc_ms);
  options.deterministic_job = true;

  if (!g_runtime->init(options)) return 0;
  g_initialized = true;
  g_jobInitialized = true;
  g_nowMs = 0;
  g_nextJobFrameMs = 0;
  return 1;
}

WASM_KEEPALIVE int sim_job_advance_to(double now_ms) {
  if (
    !g_jobInitialized || !g_runtime || !g_initialized || now_ms < 0 || now_ms > 4294967295.0 ||
    now_ms != static_cast<double>(static_cast<uint32_t>(now_ms))
  ) {
    return 0;
  }

  const uint32_t target = static_cast<uint32_t>(now_ms);
  if (target < g_nowMs) return 0;

  while (g_nextJobFrameMs <= target) {
    g_runtime->tick(g_nextJobFrameMs);
    g_runtime->copyFrameRgb(g_framebuffer);
    if (g_nextJobFrameMs > UINT32_MAX - JOB_FRAME_MS) break;
    g_nextJobFrameMs += JOB_FRAME_MS;
  }
  g_nowMs = target;
  return 1;
}

WASM_KEEPALIVE const uint8_t* sim_job_framebuffer() {
  return g_jobInitialized ? g_framebuffer : nullptr;
}

WASM_KEEPALIVE int sim_job_framebuffer_size() {
  return g_jobInitialized ? static_cast<int>(FRAMEBUFFER_SIZE) : 0;
}

WASM_KEEPALIVE int sim_job_output_brightness() {
  return g_jobInitialized && g_runtime ? g_runtime->outputBrightness() : -1;
}

WASM_KEEPALIVE int sim_job_effect_id() {
  return g_jobInitialized && g_runtime ? Effects::toIndex(g_runtime->activeEffect()) : -1;
}

WASM_KEEPALIVE int sim_job_palette_id() {
  return g_jobInitialized && g_runtime ? Palettes::toIndex(g_runtime->palette()) : -1;
}

WASM_KEEPALIVE int sim_job_brightness() {
  return g_jobInitialized && g_runtime ? g_runtime->effectBrightness() : -1;
}

WASM_KEEPALIVE int sim_job_speed() {
  return g_jobInitialized && g_runtime ? g_runtime->effectSpeed() : -1;
}

WASM_KEEPALIVE int sim_job_scale() {
  return g_jobInitialized && g_runtime ? g_runtime->effectScale() : -1;
}

WASM_KEEPALIVE int sim_job_frame_ms() {
  return JOB_FRAME_MS;
}

WASM_KEEPALIVE int sim_active_effect_id() {
  lazyInit();
  return g_runtime ? Effects::toIndex(g_runtime->activeEffect()) : -1;
}

WASM_KEEPALIVE int sim_clock_is_deterministic() {
  return sim_uses_deterministic_clock() ? 1 : 0;
}

WASM_KEEPALIVE int sim_time_hours() {
  lazyInit();
  return g_runtime ? g_runtime->timeHours() : -1;
}

WASM_KEEPALIVE int sim_time_minutes() {
  lazyInit();
  return g_runtime ? g_runtime->timeMinutes() : -1;
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
