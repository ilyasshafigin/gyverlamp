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

## Сравнение влияния звука

Для синхронной анимации «без аудиореакции / с аудиореакцией» есть отдельный
набор инструментов в `sim/tools/audio_compare/`. Он не расширяет WASM-экспортёр:
аудиосценарий в формате его `request.json` по-прежнему не поддерживается.

Сборщику GIF нужны Python 3.9+ и Pillow. Для готового демонстрационного захвата
также нужен C++17-компилятор. Этот пример компилирует текущий код одного эффекта
с host-shim'ами, выполняет два независимых прогона с одинаковыми начальными
условиями и собирает GIF:

```bash
python3 sim/tools/audio_compare/run_example.py \
  --out .artifacts/audio-compare/example
```

Если Pillow не установлен, можно подготовить отдельное окружение, не меняя
системный Python:

```bash
python3 -m venv .artifacts/venvs/audio-compare
.artifacts/venvs/audio-compare/bin/python -m pip install \
  -r sim/tools/audio_compare/requirements.txt
.artifacts/venvs/audio-compare/bin/python sim/tools/audio_compare/run_example.py \
  --out .artifacts/audio-compare/example
```

Пример использует seed `12345`, матрицу 16×16 и 20 секунд по 20 кадров/с.
В аудиосценарии после начальной тишины идут отдельные импульсы Bass, Level и
Treble, затем их совместная работа и затихание. Изолированные каналы специально
синтетические: в настоящем звуке они могут быть связаны между собой. Параметры
и путь использованного сценария сохраняются в метаданных захвата.

В выходном каталоге сохраняются `off.rgb`, `on.rgb`, `audio.json`,
`capture.json`, `comparison.gif` и `report.json`, а также файлы native-сборки.
GIF показывает два эффекта на одной временной шкале и текущие значения трёх
аудиосигналов. Сырые RGB нужны для точного сравнения: GIF ограничивает палитру.
Настройки и сведения о происхождении кадров сохраняются в метаданных.
Для проверки воспроизводимости повторите команду с другим `--out` и сравните
хэши RGB и GIF, а не метаданные, содержащие пути вывода.
Для побайтовой повторяемости GIF нужны одинаковые версия Pillow и шрифт:
смена окружения может изменить GIF, не меняя исходные RGB-кадры.

Сборщик GIF не привязан к конкретному эффекту. Для любой подготовленной пары
записей с общей временной шкалой:

```bash
python3 sim/tools/audio_compare/compose.py \
  --manifest path/to/capture.json --out .artifacts/audio-compare/custom
```

Минимальный `capture.json` для двух подготовленных записей:

```json
{
  "schemaVersion": 1,
  "width": 16,
  "height": 16,
  "frameMs": 50,
  "frameCount": 400,
  "rowOrder": "top-to-bottom",
  "audio": "audio.json",
  "left": { "label": "Без аудиореакции", "rgb": "off.rgb" },
  "right": { "label": "С аудиореакцией", "rgb": "on.rgb" },
  "provenance": "Native production · synthetic AudioFrame",
  "fidelityNote": "Host shims; не запись устройства или микрофона"
}
```

Обе записи содержат по `width * height * 3 * frameCount` байт RGB24; строки
изображения идут сверху вниз. `audio.json` — массив из `frameCount` объектов
`{"frame": 0, "ms": 0, "level": 0, "bass": 0, "treble": 0, "beat": false}`.
Индекс `frame` начинается с нуля, `ms` равен `frame * frameMs`, значения трёх
сигналов — целые 0–255, `beat` — boolean. Для точного времени GIF шаг `frameMs`
должен быть кратен 10 мс. Относительные пути отсчитываются от манифеста.
Для офлайн-прототипа укажите его настоящее происхождение в `provenance` и
`fidelityNote`, а не копируйте подпись native-примера.

Демонстрационный захват фиксированный, а не универсальный запускатель эффектов:
для другого эффекта нужно отдельно подготовить входные записи. Его реализация
описана в скилле `debug-effect-sim`. Скрипт примера принимает `--fixture` для
другого аудиосценария и `--cxx` для пути к компилятору; по умолчанию проверяется
`CXX`, затем clang++/c++.

Инструмент использует системный шрифт с кириллицей; при необходимости задайте
файл шрифта через `--font`. Общий сборщик, демонстрационный пример захвата
и тесты самого сборщика хранятся в `sim/tools/audio_compare/`. Для этого примера
старые материалы сессий не нужны. Инструкция для агента включена в скилл
`debug-effect-sim`.

Разовые скрипты захвата, прототипы, локальные сценарии и визуальные результаты
хранятся в `.artifacts/audio-compare/<experiment>/`; временные тесты эффектов,
их C++-harness'ы, сборки и отчёты — отдельно в
`.artifacts/tests/audio-compare/<experiment>/`. Это касается и исходников `.py`
и `.cpp`, а не только сгенерированных файлов. Новые постоянные адаптеры, тесты
или общие fixtures добавляются только по отдельному согласованию.

Демонстрационный пример даёт вывод кода эффекта через приближённые native
host-shim'ы с синтетическим AudioFrame после микрофонной обработки,
**не запись устройства**.
Задержка микрофона, аппаратный FastLED, калибровка и ограничение тока здесь не
проверяются. Офлайн-прототипы, подготовленные для общего сборщика, необходимо
помечать отдельно: сборка GIF сама по себе не доказывает исполнение прошивки.

Адресные тесты инструмента:

```bash
python3 -m unittest discover -s sim/tools/audio_compare -p 'test_*.py' -v
```

Если изменены только эти скрипты, достаточно тестов и свежего native-примера;
пересборка прошивки и WASM не требуется.

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
