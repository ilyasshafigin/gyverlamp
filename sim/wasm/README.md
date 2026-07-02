# Сборка WASM-симулятора

Эта директория содержит тонкую Emscripten-обёртку над общим симуляторным
runtime'ом (`sim/common/sim_runtime.{h,cpp}`). Обёртка экспортирует C ABI,
который web UI загружает в браузере и использует для отрисовки эффектов.

## Требования

- Установленный и активированный [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html).
- `emcmake` и `emcc` должны быть в `PATH`.

## Сборка

Из корня проекта:

```bash
sim/wasm/build.sh
```

Или через npm из `sim/web`:

```bash
cd sim/web
npm run build:wasm
```

Скрипт конфигурирует CMake из `sim/` (`emcmake cmake -S sim -B sim/wasm/build`)
и собирает только цель `gyverlamp_sim_wasm`. Нативный CLI-runner больше не
собирается.

## Результат

После успешной сборки появляются файлы:

- `sim/web/public/wasm/gyverlamp_sim_wasm.js`
- `sim/web/public/wasm/gyverlamp_sim_wasm.wasm`

Их раздаёт `sim/web/server.js`, а UI загружает как WASM-модуль.

## Если тулчейн не установлен

Если Emscripten не найден, скрипт выводит:

```text
Error: emcmake not found. Emscripten is required to build the WASM simulator.
```

и инструкцию по установке/активации SDK. Следуйте ей и перезапустите скрипт.

## Экспортированный C ABI

Основные функции смотрите в `sim/wasm/src/wasm_main.cpp` и
`sim/wasm/exported_functions.txt`. Они объявлены как `extern "C"` и помечены
`EMSCRIPTEN_KEEPALIVE`.

Ключевые экспорты:

- `int sim_init()` — инициализировать runtime и framebuffer.
- `int sim_update(double now_ms)` — обновить состояние эффектов и контроллеров
  без отрисовки.
- `int sim_render()` — отрисовать один кадр и заполнить framebuffer.
- `int sim_tick(double now_ms)` — совместимый хелпер: update + render.
- `uint8_t* sim_framebuffer()` — указатель на RGB framebuffer.
- `int sim_width()`, `int sim_height()`
- `int sim_set_effect(int id)`
- `int sim_set_palette(int id)`
- `int sim_set_brightness(int)`, `int sim_set_speed(int)`, `int sim_set_scale(int)`
- `int sim_reset_defaults()`
- `int sim_power(int mode)`
- `int sim_effect_next()`, `int sim_effect_prev()`
- `int sim_notify_text(const char*)`, `int sim_notify_user(int)`,
  `int sim_notify_clear()`
- `int sim_button_press(int)`, `int sim_button_release()`, `int sim_button_tap(int)`,
  `int sim_button_hold()`, `int sim_button_step()`
- Метаданные по display-индексу:
  - `uint8_t sim_effect_count()`
  - `int sim_effect_id_at(int index)`
  - `const char* sim_effect_name_at(int index)`
  - `uint8_t sim_effect_default_brightness(int index)`
  - `uint8_t sim_effect_default_speed(int index)`
  - `uint8_t sim_effect_default_scale(int index)`
  - `uint8_t sim_effect_reset_on_change(int index)`
  - `uint8_t sim_palette_count()`
  - `int sim_palette_id_at(int index)`
  - `const char* sim_palette_name_at(int index)`

Обёртка лениво инициализирует runtime/framebuffer, так что геттеры, сеттеры,
`tick`, `update` и `render` не падают до вызова `sim_init()`. Вызов `sim_tick()`,
`sim_update()` или `sim_render()` без `sim_init()` сначала выполняет
инициализацию по умолчанию. `sim_init()` можно вызывать повторно; он обнуляет
framebuffer.

## Архитектура

- `sim/wasm/src/wasm_main.cpp` — только C ABI и конвертация команд в
  `sim::Command`.
- `sim/common/sim_runtime.{h,cpp}` — production визуальный граф:
  `FrameRenderer`, `PowerController`, `EffectController`, notification и т.д.
- `sim/host/` — host-реализации `Led`, `TimeService` и shim'ы Arduino/FastLED,
  общие для сборки WASM.

Никакой нативный CLI-runner и WebSocket-шлюз не используются: весь симулятор
работает в браузере через WASM.
