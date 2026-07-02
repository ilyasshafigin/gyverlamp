# Симулятор GyverLamp (WASM)

Локальный браузерный симулятор для разработки и просмотра эффектов без лампы
и без прошивки устройства. Собирает настоящий каталог эффектов из `src/effect`
через host-shim'ы Arduino/FastLED в WASM-модуль, который выполняется в браузере.

## Структура

- `sim/wasm/` — Emscripten-обёртка и скрипт сборки WASM-модуля.
- `sim/web/` — статический Node-сервер (`server.js`) и web UI (`public/index.html` + `public/app.js`).
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
- Симулятор работает только в браузере через WASM; нативный CLI-runner и
  WebSocket-шлюз удалены.
