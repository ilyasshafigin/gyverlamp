# Симулятор GyverLamp (WASM)

Локальный браузерный симулятор для разработки и просмотра эффектов без лампы
и без прошивки устройства. Собирает настоящий каталог эффектов из `src/effect`
через host-shim'ы Arduino/FastLED в WASM-модуль, который выполняется в браузере.

## Структура

- `sim/wasm/` — Emscripten-обёртка и скрипт сборки WASM-модуля.
- `sim/web/` — статический Node-сервер (`server.js`), web UI (`public/index.html` + `public/app.js`) и
  детерминированный CLI-экспортёр в `tools/`.
- `sim/common/` — общий симуляторный runtime: `sim_runtime.{h,cpp}` и
  `sim_time.{h,cpp}`.
- `sim/host/` — host-реализации и shim'ы для прошивочного кода:
  - `host/src/host_led.cpp` — реализация `Led` на стороне хоста;
  - `host/src/sim_stubs.cpp` — host-заглушки `TimeService`;
  - `host/shims/` — Arduino/FastLED/EEPROM/WString shim'ы.

## Быстрый запуск

Из `sim/web`:

```bash
cd sim/web
npm install
npm run sim
# или из корня проекта
just run sim
```

Открыть http://localhost:8080.

`npm run sim` сначала собирает WASM-артефакты (`npm run build:wasm`), потом
запускает статический сервер. UI загружает `wasm/gyverlamp_sim_wasm.js` и
`.wasm` относительными путями, поэтому работает и локально, и на GitHub Pages
project site.

Если WASM-артефакты отсутствуют, UI покажет команду `npm run build:wasm` и не
упадёт пустой страницей.

## Сборка WASM модуля

Требуется [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html).

Из корня проекта:

```bash
sim/wasm/build.sh
```

Или из `sim/web`:

```bash
cd sim/web
npm run build:wasm
```

Скрипт использует `emcmake cmake -S sim -B sim/wasm/build` и собирает только
цель `gyverlamp_sim_wasm`. Если `emcmake`/`emcc` не найдены, выводится
инструкция по установке SDK.

Выходные файлы (не коммитятся):

- `sim/web/public/wasm/gyverlamp_sim_wasm.js`
- `sim/web/public/wasm/gyverlamp_sim_wasm.wasm`

## Публикация на GitHub Pages

Workflow `.github/workflows/sim-pages.yml` собирает и публикует симулятор:

1. ставит Node.js и Emscripten;
2. выполняет `npm ci --prefix sim/web`;
3. выполняет `npm run build:wasm --prefix sim/web`;
4. загружает `sim/web/public` как GitHub Pages artifact;
5. деплоит artifact через `actions/deploy-pages`.

Workflow запускается на `push` в `main` при изменениях в `sim/**`, `src/**` или
самом workflow, а также вручную через `workflow_dispatch`.

В настройках репозитория Pages должен быть выбран источник **GitHub Actions**.

## Переменные окружения сборки

Размер матрицы и геометрия ленты задаются на этапе компиляции через env-переменные:

- `SIM_WIDTH` — ширина матрицы;
- `SIM_HEIGHT` — высота матрицы;
- `SIM_CONNECTION_ANGLE` — угол подключения ленты;
- `SIM_STRIP_DIRECTION` — направление ленты.

После изменения любой из них нужна пересборка WASM.

Пример:

```bash
SIM_WIDTH=32 SIM_HEIGHT=16 npm run build:wasm
```

По умолчанию используются значения из `src/config.h`.

## Детерминированный экспорт эффектов

CLI загружает один свежий WASM-модуль на одну задачу. Сначала соберите WASM и
установите зависимости:

```bash
cd sim/web
npm ci
npm run build:wasm
npm run render:catalog -- --json
npm run render:effect -- --request request.json --out ../../.artifacts
```

`render:catalog` и успешный `render:effect` печатают ровно один JSON в stdout;
диагностика уходит в stderr. Пути к WASM вычисляются от расположения скрипта,
поэтому команды работают из произвольного текущего каталога. Результат эффекта
публикуется атомарно в `<out>/<jobId>/`; неполная задача не публикуется.

Схема `request.json` (неизвестные поля запрещены):

```json
{
  "schemaVersion": 1,
  "effectId": 0,
  "expectedEffectName": "Color",
  "paletteId": 0,
  "brightness": 255,
  "speed": 128,
  "scale": 128,
  "seed": 12345,
  "clockStartUtc": "2024-01-02T03:04:05.006Z",
  "atMs": [0, 1000, 2000],
  "views": ["logical", "sharp-v1", "diffuser-v1"],
  "previewScale": 16
}
```

Обязательны `schemaVersion`, `effectId`, `seed`, `clockStartUtc`, `atMs`,
`views`. `clockStartUtc` строго использует формат UTC
`YYYY-MM-DDTHH:mm:ss.sssZ`; `atMs` — отсортированный уникальный список от 0 до
600000 мс. Максимум 120 captures и 30001 внутренних frame-boundary шагов.
`seed` находится в `1..4294967295`: ноль отклоняется, поэтому manifest всегда
фиксирует фактически использованный seed. Cadence берётся из export ABI
`sim_job_frame_ms()` (`FRAME_MS` прошивки), до `sim_job_init()`; Node не хранит
свою копию cadence.
Пропущенные palette и effect-параметры берутся из WASM-каталога. Неверное
ожидаемое имя эффекта завершает задачу до публикации.

Сборка создаёт `gyverlamp_sim_wasm.identity.json`: fingerprint simulator/firmware
compile inputs, фактические geometry/toolchain параметры и SHA-256 WASM/glue JS.
CLI сверяет текущий source fingerprint и оба artifact hash; toolchain и `SIM_*`
environment при render не читаются. Geometry берётся из loaded WASM/sidecar.
Stale/missing binary отклоняется с командой rebuild. `manifest.json` содержит
канонический resolved request, SHA-256 WASM и identity, job ID,
геометрию и порядок строк (`bottom-row-first`), хэши RGB, размеры/пути PNG и
предупреждения fidelity. `logical` — исходная матрица RGB до output brightness;
`sharp-v1` — nearest-neighbor preview с applied brightness;
`diffuser-v1` — CPU profile с linear RGB, X wrap, Y clamp и фиксированными
kernel/LUT. CRGB bytes считаются linear LED intensity (`byte / 255`), brightness
применяется в linear space, а sRGB — только финальный encode. Blur radius задан
в LED pitch и не зависит от `previewScale`. Exporter создаёт только requested
views; contact sheet выбирает `diffuser-v1`, затем `sharp-v1`, затем `logical` и
записывает source level. До allocations проверяется budget 134217728 bytes:
captured RGB/RGBA frames, linear diffusion buffers, contact sheet, `Buffer.from`,
pngjs filter/deflate/result/`Buffer.concat` reserve. Каждый frame charge считается
одновременно: admission не зависит от timing GC.
Profile приближённый: current limiting и аппаратная калибровка не моделируются.
`.artifacts/` игнорируется Git.

## Web UI

В UI доступны:

- выбор эффекта;
- выбор палитры;
- `Brightness`, `Speed`, `Scale`, `FPS`;
- **Reset defaults**;
- **Pause / Resume**;
- **Step** — отрисовать один кадр в паузе;
- preview-настройки diffuser.

Состояние эффекта, палитры, слайдеров, FPS и preview-настроек сохраняется в
`localStorage` браузера и восстанавливается при загрузке страницы.

## Preview diffuser

Diffuser-настройки влияют только на отображение в браузере. Логика эффектов не
меняется.

- `Diffuser` переключает резкий pixel preview на размытый вид лампы.
- `LED gap` добавляет расстояние между виртуальными светодиодами перед blur,
  как у реальной матрицы за белым плафоном.
- `Blur` задаёт силу размытия в пикселях canvas.

## Не коммитить

- `sim/wasm/build/`
- `sim/web/public/wasm/`
- `sim/web/node_modules/`

Эти директории содержат генерируемые артефакты и уже добавлены в `.gitignore`.

## Ограничения

- Arduino и FastLED представлены host-shim'ами, а не настоящими библиотеками.
- Цвета и математика FastLED приближены к оригиналу, но не гарантируют бит-в-бит
  совпадение с ESP8266.
- Вывод на реальную LED-ленту (`FastLED.show`) не используется.
- Equalizer работает на fake audio внутри симулятора, полноценного аудио-входа
  нет.
- Экспортёр исполняет тот же WASM-модуль в Node.js; нативный CLI-runner и
  WebSocket-шлюз не используются.
