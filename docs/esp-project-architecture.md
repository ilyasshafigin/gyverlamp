# Архитектура универсального ESP-проекта

## Назначение документа

Этот документ задаёт принцип построения прошивки для ESP8266/ESP32 в PlatformIO. Он предназначен как техническое задание агенту, который создаёт новый проект или приводит существующий проект к этой архитектуре.

Нужно переносить не названия и предметную логику GyverLamp, а способ организации программы:

- тонкая точка входа;
- один корневой объект приложения;
- явное владение сервисами и передача зависимостей через конструкторы;
- разделение прикладной логики, оборудования, сети и постоянного хранения;
- неблокирующий жизненный цикл `init()`/`tick()`;
- единое управление EEPROM layout;
- отдельный RAM-репозиторий изменяемых настроек;
- независимые адаптеры Web и MQTT над общей моделью устройства;
- обязательные WiFi и OTA;
- разделение общей PlatformIO-конфигурации и локальных параметров конкретных устройств.

Конкретные названия классов зависят от назначения устройства. `App`, `DeviceController` и `DeviceState` ниже — условные имена. В проекте лампы их аналогами служат `Lamp`, контроллеры питания/эффектов и состояние лампы.

## Основные свойства архитектуры

### Один владелец графа объектов

Корневой класс приложения создаёт и хранит все долгоживущие компоненты. Он выступает composition root:

- определяет время жизни объектов;
- передаёт ссылки на зависимости в конструкторы;
- задаёт порядок инициализации;
- вызывает периодические методы в главном цикле;
- связывает прикладные изменения с публикацией состояния наружу.

Компоненты не должны создавать друг друга через `new`, искать глобальные singleton-объекты или повторно владеть общей зависимостью. Если `WebService` и `MqttService` управляют одним устройством, оба получают ссылку на один и тот же прикладной контроллер.

Для микроконтроллера предпочтительно статическое время жизни объектов. Динамическое выделение памяти в рабочем цикле следует минимизировать.

### Кооперативный неблокирующий цикл

Каждый длительно живущий компонент имеет два основных метода:

- `init()` — одноразовая настройка после запуска;
- `tick()` — короткая порция периодической работы без длительного ожидания.

Допустимы более предметные имена вроде `render()`, `poll()` или `update()`, если они точнее выражают операцию. Главное требование: основной `loop()` не должен надолго блокироваться.

Вместо `delay()` для повторных попыток, таймаутов и отложенного сохранения используются `millis()`, небольшие timer-классы или конечные автоматы. После обслуживания всех компонентов цикл должен вернуть управление ESP runtime; для ESP8266 обычно вызывается `yield()`.

### Явные зависимости

Зависимости передаются ссылками через конструктор:

```cpp
class WebService {
public:
  WebService(
    EepromStore& eeprom,
    SettingsRepository& settings,
    DeviceController& device,
    MqttService& mqtt,
    WifiService& wifi
  );
};
```

Порядок полей корневого класса должен соответствовать порядку их конструирования. Базовые компоненты объявляются раньше потребителей:

1. аппаратные драйверы и storage;
2. модель настроек и прикладные контроллеры;
3. сетевые сервисы;
4. интерфейсы управления, зависящие от остальных компонентов.

Нужно избегать циклических зависимостей. Если Web и MQTT должны сообщать, что состояние изменилось, лучше использовать небольшой общий change tracker, dirty-флаг или метод прикладного контроллера, а не связывать Web и MQTT друг с другом в обе стороны.

## Рекомендуемая структура каталогов

```text
project/
├── platformio.ini
├── platformio.local.example.ini
├── platformio.local.ini             # локальный, не хранится в Git
├── extra_script.py                   # при необходимости генерации compile_commands.json
├── Justfile                          # единые команды build/format/tidy/upload
├── .clang-format
├── .clang-tidy
├── .editorconfig
├── .gitignore
└── src/
    ├── main.cpp
    ├── config.h
    ├── core/
    │   ├── app.h
    │   ├── app.cpp
    │   ├── device_controller.h
    │   ├── device_controller.cpp
    │   ├── state_notifier.h
    │   └── <другие прикладные контроллеры>
    ├── hardware/
    │   ├── button.h
    │   ├── button.cpp
    │   ├── led.h
    │   ├── led.cpp
    │   └── <датчики, реле, дисплеи, приводы>
    ├── network/
    │   ├── wifi_config.h
    │   ├── wifi_service.h
    │   ├── wifi_service.cpp
    │   ├── ota_service.h
    │   ├── ota_service.cpp
    │   ├── mqtt_config.h
    │   ├── mqtt_service.h
    │   ├── mqtt_service.cpp
    │   ├── web_service.h
    │   └── web_service.cpp
    ├── storage/
    │   ├── eeprom_layout.h
    │   ├── eeprom_store.h
    │   ├── eeprom_store.cpp
    │   ├── settings_repository.h
    │   └── settings_repository.cpp
    ├── util/
    │   ├── timer.h
    │   ├── periodic_timer.h
    │   └── <малые общие утилиты>
    └── <domain>/
        └── <предметные классы нового устройства>
```

Это схема ответственности, а не требование создавать пустые каталоги. Каталог появляется, когда в нём есть самостоятельный модуль.

Предметный каталог зависит от устройства. Для светильника это могут быть эффекты и рендеринг, для контроллера климата — измерения, регулятор и сценарии, для реле — каналы и расписания.

## Точка входа и корневой класс

### `src/main.cpp`

`main.cpp` должен быть минимальным. В нём нельзя размещать сетевую, аппаратную или прикладную логику.

```cpp
#include <Arduino.h>
#include "core/app.h"

static App app;

void setup() {
  Serial.begin(115200);
  app.setup();
}

void loop() {
  app.loop();
}
```

Платформенные директивы вроде `ADC_MODE()` допустимы здесь, если они должны находиться на уровне translation unit и не являются логикой приложения.

### `core/App`

`App` владеет всеми сервисами. Для статического dependency graph удобно использовать default member initializers с brace initialization. Отдельный пустой constructor не нужен:

```cpp
class App {
public:
  void setup();
  void loop();

private:
  EepromStore eeprom_;
  SettingsRepository settings_{eeprom_};

  Led led_;
  Button button_;

  StateNotifier stateNotifier_;
  DeviceController device_{led_, settings_, stateNotifier_};

  WifiService wifi_{eeprom_};
  OtaService ota_{wifi_};
  MqttService mqtt_{eeprom_, device_, settings_, wifi_};
  SettingsAsync webSettings_;
  WebService web_{eeprom_, settings_, device_, mqtt_, webSettings_, wifi_};
};
```

Компилятор создаст implicit default constructor `App::App()`. Это пример состава, не готовый API. Нужно сохранить принцип и заменить предметные зависимости.

Порядок строк в `private` — часть семантики. C++ всегда конструирует fields по порядку объявления, независимо от порядка их упоминания где-либо ещё. Поэтому `eeprom_` существует до `settings_`, `wifi_` — до `ota_`, а `web_` создаётся последним. Разрушение идёт в обратном порядке: потребители уничтожаются раньше зависимостей.

### Порядок `setup()`

Рекомендуемый порядок:

1. безопасно настроить GPIO и аппаратные драйверы;
2. открыть EEPROM и проверить layout;
3. загрузить настройки в RAM;
4. инициализировать прикладные контроллеры;
5. запустить WiFi state machine;
6. настроить OTA callbacks;
7. настроить MQTT entities/callbacks;
8. запустить Web UI;
9. запустить остальные таймеры и фоновые сервисы.

Точный порядок определяется зависимостями. Компонент нельзя инициализировать раньше данных или сервиса, которые он читает в `init()`.

### Порядок `loop()`

Пример:

```cpp
void App::loop() {
  device.tick();
  settings.tick();
  button.tick();

  wifi.tick();
  ota.tick();
  web.tick();
  mqtt.tick();

  if (stateNotifier.consumeChanged()) {
    mqtt.updateStates();
  }

  yield();
}
```

Порядок должен быть осмысленным. Сначала обновляется фактическое состояние, затем обслуживаются интерфейсы, после этого публикуется уже актуальный snapshot.

## Прикладной слой `core/`

`core/` содержит правила поведения устройства, но не детали HTTP, MQTT topics или EEPROM-адресов.

Прикладной контроллер должен предоставлять единый API для всех источников управления:

```cpp
device.setPower(true);
device.setTargetValue(42);
device.activateMode(DeviceMode::Automatic);
```

Кнопка, Web UI и MQTT должны вызывать одинаковые операции. Нельзя реализовывать три независимые версии переключения питания или изменения режима.

Контроллер отвечает за:

- проверку и нормализацию входных значений;
- согласованное изменение связанных параметров;
- управление оборудованием через hardware-классы;
- обновление RAM-настроек;
- отметку изменённого состояния;
- предоставление текущего фактического состояния.

### Сигнал изменения состояния

Для публикации изменений наружу полезен маленький `StateNotifier` или эквивалентный dirty tracker:

```cpp
class StateNotifier {
public:
  void markChanged() { changed_ = true; }

  bool consumeChanged() {
    const bool changed = changed_;
    changed_ = false;
    return changed;
  }

private:
  bool changed_ = false;
};
```

Он не хранит состояние устройства. Он сообщает, что после локальной кнопки, Web-команды, таймера или внутренней автоматики нужно синхронизировать внешние представления.

Если несколько потребителей должны независимо получить событие, один `consumeChanged()` недостаточен. Тогда нужны generation counter, отдельные dirty-флаги или небольшой event dispatcher.

## Аппаратный слой `hardware/`

Каждое физическое устройство получает отдельный класс: кнопка, адресная лента, обычный LED, реле, датчик, микрофон, дисплей, двигатель.

Hardware-класс инкапсулирует:

- номера и режимы GPIO;
- настройку сторонней библиотеки;
- чтение сырого сигнала;
- debounce, фильтрацию и преобразование единиц;
- безопасное начальное состояние;
- непосредственную отправку значения на периферию.

Пример API:

```cpp
class Relay {
public:
  explicit Relay(uint8_t pin);
  void init();
  void set(bool enabled);
  bool isEnabled() const;
};
```

По возможности hardware-класс не должен знать MQTT, Web UI, EEPROM layout или весь объект приложения.

Для кнопки допустимы два уровня:

- низкоуровневый `Button`, который выдаёт click/hold события;
- прикладной `ButtonController`, который переводит их в команды устройства.

В маленьком проекте уровни можно объединить, но зависимости должны оставаться явными. Если класс кнопки напрямую вызывает `DeviceController`, он всё равно не должен знать MQTT/Web и не должен дублировать правила изменения состояния.

GPIO и электрические параметры задаются централизованно через `config.h` или build flags, а не размножаются по `.cpp`.

## Постоянное хранение

### Разделение ответственности

Хранение делится на три части:

1. структуры данных, например `WifiConfig`, `MqttConfig`, `DeviceSettings`;
2. `eeprom_layout.h` с физической картой адресов;
3. `EepromStore`, выполняющий чтение, запись, commit, defaults и миграцию.

Изменяемая рабочая копия настроек хранится отдельно в `SettingsRepository`.

### Конфигурационные структуры

Структуры, записываемые в EEPROM, должны иметь предсказуемый размер. Для строк использовать фиксированные массивы:

```cpp
constexpr uint8_t WIFI_SSID_LEN = 33;
constexpr uint8_t WIFI_PASS_LEN = 65;

struct WifiConfig {
  char ssid[WIFI_SSID_LEN];
  char password[WIFI_PASS_LEN];
};
```

При копировании строк использовать `strlcpy` или эквивалент с гарантированной нуль-терминацией. `String` нельзя записывать в EEPROM как сырой объект.

Для persisted-структур полезны проверки:

```cpp
static_assert(sizeof(DeviceSettings) == EXPECTED_SIZE,
              "EEPROM layout expects fixed DeviceSettings size");
```

Нужно учитывать padding C++-структур. Если бинарная совместимость критична, хранить поля по отдельности или явно контролировать packing и типы.

### `eeprom_layout.h`

В одном файле должны находиться:

- общий `EEPROM_SIZE`;
- magic layout;
- текущая версия;
- начало и размер каждого блока;
- адреса полей;
- резервные области;
- capacity массивов;
- `static_assert` для границ и пересечений.

Принцип карты:

```text
0..N       metadata: magic + version
...        reserve
...        WiFi config block
...        MQTT config block
...        device state block
...        domain settings block
...        reserve for future versions
```

Адреса нельзя объявлять в `WifiService`, `MqttService`, `WebService` или предметных контроллерах.

Добавление поля требует:

1. выбрать место в layout;
2. проверить вместимость блока;
3. обновить compile-time проверки;
4. увеличить версию, если старое содержимое интерпретируется иначе;
5. определить default;
6. определить, переносится ли старое значение при миграции.

### Версия и миграция

При `EepromStore::init()`:

1. вызывается `EEPROM.begin(EEPROM_SIZE)`;
2. читаются magic и version;
3. при отсутствии валидного magic записывается чистый layout с defaults;
4. при старой версии выполняется явная миграция;
5. после успешной инициализации записывается текущая версия.

Политика миграции должна быть простой и видимой. Допустим практичный вариант:

- сохранить критичные пользовательские параметры в RAM;
- очистить layout;
- записать defaults новой версии;
- восстановить совместимые WiFi/MQTT credentials и пользовательские параметры;
- не восстанавливать данные, формат которых изменился.

Нельзя молча читать старую карту новыми адресами.

### `EepromStore`

`EepromStore` предоставляет предметные операции, а не произвольные EEPROM-адреса:

```cpp
const WifiConfig& readWifiConfig();
bool writeWifiConfig(const char* ssid, const char* password);

const MqttConfig& readMqttConfig();
bool writeMqttConfig(const char* host, const char* port,
                     const char* user, const char* password);

DeviceSettings readDeviceSettings();
bool writeDeviceSettings(const DeviceSettings& settings);
```

Потребители не должны напрямую вызывать `EEPROM.read()`, `EEPROM.put()` или `EEPROM.commit()`.

Если метод возвращает ссылку на внутренний cache, время жизни и перезапись cache должны быть понятны вызывающему коду. Для маленьких структур возврат по значению часто безопаснее.

Credentials и редкие системные настройки можно сохранять немедленно. Часто меняющиеся параметры нужно направлять через `SettingsRepository`.

## `SettingsRepository`

`SettingsRepository` хранит рабочие настройки в RAM и уменьшает износ flash.

Он отвечает за:

- загрузку настроек из `EepromStore` в `init()`;
- доступ к изменяемым и `const` значениям;
- определение изменений;
- dirty-флаг;
- отложенную запись;
- сохранение согласованного snapshot.

Типичная схема:

```cpp
class SettingsRepository {
public:
  explicit SettingsRepository(EepromStore& eeprom);

  void init();
  void tick();

  DeviceSettings& device();
  const DeviceSettings& device() const;

  void markDirty();
  bool saveNow();

private:
  EepromStore& eeprom_;
  DeviceSettings deviceSettings_{};
  bool dirty_ = false;
  uint32_t changedAt_ = 0;
};
```

После изменения настройка остаётся в RAM и сразу влияет на устройство. Запись выполняется после периода покоя, например через 30 секунд после последнего изменения. Новое изменение перезапускает отсчёт.

Не следует полагаться только на сравнение одного текущего объекта, если репозиторий хранит несколько наборов настроек. Нужен dirty state, snapshot или проверка каждого сохраняемого набора.

Перед операцией, после которой питание или прошивка могут перезапуститься, важные накопленные изменения следует сохранить принудительно.

## `WifiService`

WiFi обязателен и переносится как отдельный неблокирующий state machine.

### Зависимости и API

Минимальная зависимость — `EepromStore`:

```cpp
class WifiService {
public:
  explicit WifiService(EepromStore& eeprom);

  void init();
  void tick();

  bool isStaConnected() const;
  const String& getDeviceId() const;
};
```

`WifiService` читает `WifiConfig`, но не знает Web UI или MQTT.

### Состояния STA

Рекомендуемые состояния:

- `Provisioning` — сохранённой STA-конфигурации нет;
- `Connecting` — идёт ограниченная по времени попытка;
- `Connected` — STA подключена;
- `RetryWait` — ожидание перед новой попыткой.

### Состояния AP

AP имеет независимые состояния:

- `Inactive`;
- `Active`;
- `RetryWait`.

Разделение важно: STA может переподключаться, пока setup AP остаётся доступной.

### Поведение `init()`

Если SSID пуст:

1. установить `WIFI_AP`;
2. открыть setup AP;
3. перейти в `Provisioning`.

Если STA credentials есть:

1. установить `WIFI_AP_STA`;
2. сразу открыть setup AP;
3. начать асинхронное STA-подключение;
4. не ждать результата внутри `init()`.

Setup AP при запуске нужен, чтобы пользователь мог исправить credentials, даже пока STA-подключение не удалось.

### Поведение `tick()`

- `Connecting`: проверить успех; после timeout перейти в `RetryWait`;
- `Connected`: при потере связи перейти в `RetryWait`;
- `RetryWait`: после интервала начать новую ограниченную попытку;
- при ошибке запуска AP повторять попытку с отдельным интервалом;
- активный AP закрывать по timeout только при отсутствии подключённых клиентов;
- при успешном STA-подключении закрыть AP;
- не использовать блокирующий цикл ожидания `WiFi.status()`.

Все интервалы должны быть именованными константами: connect timeout, reconnect interval, AP retry interval, AP inactivity timeout.

`stopAp()` должен корректно выбрать оставшийся режим:

- `WIFI_STA`, если сохранена STA-конфигурация;
- выключить WiFi или выбрать проектный режим, если STA-конфигурации нет и provisioning завершён.

Device ID, AP SSID и hostname формируются из build-time `DEVICE_NAME` либо стабильного chip ID. Они не должны быть случайными после каждой перезагрузки.

## `OtaService`

OTA обязателен как отдельный сервис. Он не должен запускать OTA-сервер до появления STA-соединения.

### Зависимость

```cpp
class OtaService {
public:
  explicit OtaService(WifiService& wifi);

  void init();
  void tick();

private:
  WifiService& wifi_;
  bool begun_ = false;
};
```

Дополнительные зависимости разрешены только для универсальных действий нового устройства: перевести выходы в безопасное состояние, приостановить привод, выполнить финальное сохранение. Они не должны затягивать OTA в предметную UI-логику.

### Жизненный цикл

`init()` регистрирует callbacks:

- start;
- end;
- progress;
- error.

Callbacks как минимум пишут понятные сообщения в Serial. На старте можно остановить операции, несовместимые с обновлением. На ошибке устройство должно остаться в безопасном состоянии.

`tick()`:

```cpp
void OtaService::tick() {
  if (!begun_) {
    if (!wifi_.isStaConnected()) return;
    ArduinoOTA.begin();
    begun_ = true;
  }

  ArduinoOTA.handle();
}
```

Такой порядок позволяет вызвать `ota.init()` до подключения WiFi и не блокирует загрузку.

Если OTA compile-time optional для некоторых сборок, класс всё равно сохраняет тот же публичный API, а при выключенном `USE_OTA` методы становятся no-op. В целевом проекте хотя бы основной environment должен собираться с `USE_OTA`.

## `MqttService`

MQTT — внешний адаптер над прикладным API устройства. Он не является владельцем состояния.

### Структура

`mqtt_config.h` содержит только размеры и структуру credentials:

```cpp
struct MqttConfig {
  char host[MQTT_HOST_LEN];
  char port[MQTT_PORT_LEN];
  char user[MQTT_USER_LEN];
  char password[MQTT_PASS_LEN];
};
```

`MqttService` получает:

- `EepromStore` для чтения конфигурации;
- `WifiService` для проверки доступности STA и стабильного device ID;
- прикладные контроллеры для выполнения команд;
- `SettingsRepository`, если MQTT изменяет сохраняемые настройки.

Сервис владеет WiFi/MQTT client objects и MQTT/HA entities.

### `init()`

В `init()` нужно:

1. прочитать MQTT config;
2. настроить device metadata и unique IDs;
3. создать или настроить entities;
4. привязать command callbacks;
5. подготовить список допустимых режимов/эффектов/опций;
6. не выполнять длительное блокирующее подключение.

Callbacks переводят протокольную команду в вызов прикладного API:

```text
MQTT payload -> parse/validate -> DeviceController method
             -> SettingsRepository dirty
             -> state changed
```

Протокольный код не должен напрямую переключать GPIO или писать EEPROM.

### `tick()` и reconnect

`tick()` отвечает за:

- выход, если STA недоступна;
- неблокирующие попытки подключения с интервалом;
- обслуживание MQTT client loop;
- публикацию dirty entities;
- восстановление discovery/state после reconnect.

Credentials могут быть пустыми. Тогда сервис должен оставаться выключенным без постоянного потока ошибок.

После изменения MQTT config через Web UI нужен явный метод вроде `reloadConfig()`, `reconnect()` или безопасная перезагрузка устройства. Нельзя продолжать использовать указатели на устаревший cache.

### Публикация состояния

`updateStates()` читает состояние у контроллеров и обновляет все представления:

- power;
- числовые параметры;
- активный режим;
- sensor values;
- доступность возможностей;
- диагностические поля.

Источником истины остаётся устройство. После входящей MQTT-команды следует публиковать нормализованное фактическое значение, а не безусловно повторять payload.

`updateStates()` может только менять локальные entity states/dirty flags; фактическую отправку выполняет MQTT library в `tick()`. Это уменьшает связанность и повторные публикации.

Если MQTT выключается флагом `USE_MQTT`, сохранить тот же публичный API и no-op реализацию. Остальной проект не должен быть заполнен `#ifdef USE_MQTT`.

## `WebService`

Web UI — второй внешний адаптер над тем же прикладным API.

### Владение и зависимости

`WebService` не хранит истинное состояние устройства. Он получает ссылки на:

- объект web/settings-библиотеки;
- `EepromStore`;
- `SettingsRepository`;
- прикладные контроллеры;
- `WifiService`;
- `MqttService`, если UI показывает MQTT status или инициирует reload;
- hardware-классы только когда UI управляет чисто аппаратной настройкой, не представленной прикладным контроллером.

Большой constructor допустим для верхнеуровневого адаптера, но каждая зависимость должна реально использоваться. Не передавать весь `App`.

### Два направления работы

WebService делится на:

- builder/render path — построение формы из текущего состояния;
- update path — разбор отправленных значений и применение команд.

При использовании `SettingsAsync` это могут быть методы:

```cpp
void settingsBuilder(sets::Builder& builder);
void settingsUpdate(sets::Updater& updater);
```

`init()` запускает сервер и регистрирует builder/update callbacks. `tick()` обслуживает библиотеку, если ей требуется периодическая работа.

### Секции UI

Страницу лучше строить логическими секциями:

- состояние и основные команды устройства;
- предметные настройки;
- hardware settings;
- WiFi;
- MQTT;
- OTA/firmware information;
- диагностика.

UI-идентификаторы полей должны быть стабильными и уникальными. Значения select/options формируются из предметных enum/catalog, а не поддерживаются отдельным несинхронизированным списком.

### Временные input buffers

Для credential fields WebService может хранить фиксированные input buffers. Перед построением формы они заполняются из `EepromStore`. При submit:

1. проверить длину и формат;
2. безопасно скопировать значения;
3. записать весь согласованный config одним методом Store;
4. сообщить пользователю, требуется ли reconnect/restart.

Пароль нельзя случайно выводить в Serial или MQTT diagnostics. Нужно учитывать, должна ли форма показывать сохранённый пароль или пустой placeholder.

### Применение команд

Обычные изменения проходят через прикладной API:

```text
Web input -> validate -> DeviceController method
          -> SettingsRepository dirty
          -> state changed
```

WebService не должен повторять внутреннюю логику контроллера и не должен напрямую менять связанные поля в обход его invariants.

## Конфигурация проекта

### `src/config.h`

`config.h` содержит общие compile-time defaults и аппаратные константы:

- GPIO;
- размеры и электрические лимиты;
- AP IP, AP password и network timeouts;
- MQTT/Web defaults, не содержащие секретов;
- firmware version fallback;
- параметры устройства, допускающие переопределение build flags.

Параметры, разные для каждого экземпляра, должны иметь fallback:

```cpp
#ifndef DEVICE_NAME
#define DEVICE_NAME "EspDevice"
#endif

#ifndef CURRENT_LIMIT
#define CURRENT_LIMIT 1000
#endif
```

Секреты, IP конкретного устройства и локальные варианты сборки не коммитятся в `config.h`.

### Разделение PlatformIO-конфигурации

Должны существовать три уровня:

1. `platformio.ini` — общая, коммитящаяся конфигурация проекта;
2. `platformio.local.example.ini` — коммитящийся пример локальной конфигурации;
3. `platformio.local.ini` — реальная локальная конфигурация разработчика, добавленная в `.gitignore`.

#### `platformio.ini`

Главный файл подключает локальный:

```ini
[platformio]
extra_configs =
  platformio.local.ini

[env]
platform = espressif8266
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs

check_tool = clangtidy
check_flags =
  clangtidy: --config-file=.clang-tidy

lib_deps =
  ; общие закреплённые зависимости

build_src_flags =
  -D FIRMWARE_VERSION=\"dev\"
```

Здесь размещаются:

- platform/framework;
- общие скорости;
- filesystem;
- общие library dependencies с закреплёнными версиями или commit hash;
- настройки компилятора;
- `check_tool` и `check_flags` для общего static-analysis ruleset;
- общие feature-independent definitions;
- подключение `extra_script.py`, если он нужен всем environments.

Здесь не должно быть:

- WiFi/MQTT credentials;
- реальных OTA IP;
- уникального `DEVICE_NAME` конкретного экземпляра;
- локального default environment разработчика;
- непереносимых путей на машине.

#### `platformio.local.ini`

Локальный файл создаётся копированием example и не хранится в Git. Он содержит реальные устройства и способы upload:

```ini
[platformio]
default_envs = device1_ota

[env]
build_flags =
  -D USE_MQTT
  -D USE_OTA
  -D GMT=3

[env:device1_base]
board = d1_mini
build_flags =
  ${env.build_flags}
  -D DEVICE_NAME=\"EspDevice1\"

[env:device1_usb]
extends = env:device1_base
upload_protocol = esptool

[env:device1_ota]
extends = env:device1_base
upload_protocol = espota
upload_port = 192.168.1.xxx
```

Для нескольких физических устройств создаются отдельные `<device>_base`, `<device>_usb`, `<device>_ota`. Общие параметры конкретного устройства находятся в base environment, transport upload — в дочерних.

Нельзя считать, что у каждого разработчика одинаковое число устройств или одинаковые IP.

#### `platformio.local.example.ini`

Example должен быть полностью рабочим шаблоном без секретов и реальных адресов:

- показывает обязательные секции;
- содержит хотя бы один base environment;
- содержит USB и OTA варианты;
- использует placeholder IP вроде `192.168.1.xxx`;
- показывает обязательный `USE_OTA`;
- содержит безопасные примерные значения лимитов и имени устройства;
- обновляется одновременно с изменениями требований к локальному файлу.

README должен явно требовать:

```sh
cp platformio.local.example.ini platformio.local.ini
```

После этого разработчик меняет board, `DEVICE_NAME`, upload IP и локальные flags.

### `.gitignore`

Минимально исключить:

```gitignore
.pio/
platformio.local.ini
compile_commands.json
.vscode/
```

Не исключать `platformio.local.example.ini`.

### `compile_commands.json`

Если проект использует clangd, сохранить воспроизводимую команду генерации для одного выбранного environment:

```sh
pio run -e device1_ota -t compiledb
```

При необходимости `extra_script.py` должен включать toolchain/framework headers и направлять базу в корень проекта. `compile_commands.json` генерируется локально и не коммитится.

### `Justfile`

`Justfile` стоит переносить как единый пользовательский интерфейс к командам проекта. Он не заменяет PlatformIO configuration: recipes только вызывают PlatformIO, clang-format, clang-tidy и локальные инструменты одинаковым способом.

Для универсального проекта безопаснее требовать явный environment. Recipe `build` без аргумента не должен незаметно собирать все environments. Recipe `upload` остаётся явным отдельным действием и никогда не вызывается из `build`, `tidy` или default recipe.

Рекомендуемый baseline:

```just
set shell := ["bash", "-euo", "pipefail", "-c"]

default:
    @just --list

build env:
    pio run -e "{{env}}"

compiledb env:
    pio run -e "{{env}}" -t compiledb

format +files:
    clang-format -i {{files}}

format-check +files:
    clang-format --dry-run -Werror {{files}}

tidy env:
    pio check -e "{{env}}" --fail-on-defect=medium

upload env:
    pio run -e "{{env}}" -t upload
```

`tidy` использует PlatformIO integration, а не прямой host `clang-tidy -p .`. Поэтому recipe получает тот же board, framework, defines и include paths, что выбранный environment.

Примеры:

```sh
just build device1_ota
just compiledb device1_ota
just format src/core/app.cpp src/network/wifi_service.cpp
just format-check src/core/app.cpp src/network/wifi_service.cpp
just tidy device1_ota
just upload device1_ota
```

Последняя команда прошивает устройство и выполняется только по явному запросу пользователя.

Если проект содержит simulator, можно добавить отдельные recipes `sim` и `build-sim`. Они не должны менять семантику firmware recipe `build`.

Текущий `Justfile` конкретного проекта может поддерживать variadic список environments для ручных операций. В документе для другого агента основной путь остаётся сфокусированным: один вызов — один environment.

### Сборка и проверка

Проверять ровно один сфокусированный environment, обычно основной OTA environment:

```sh
pio run -e device1_ota
```

Не запускать без необходимости сборку всех локальных environments. Не выполнять upload как часть обычной проверки. Flash/OTA upload разрешён только отдельным явным действием.

## Feature flags

Опциональные возможности включаются build flags:

```ini
-D USE_MQTT
-D USE_OTA
-D USE_BUTTON
```

Условная компиляция должна быть локализована внутри соответствующего сервиса. Для выключенной возможности сохраняется тот же класс и публичный API с no-op методами. Корневой `App` остаётся читаемым и не превращается в набор вложенных `#ifdef`.

WiFi остаётся базовой возможностью. OTA обязателен для основной целевой конфигурации.

## Потоки данных

### Загрузка

```text
EEPROM bytes
  -> EepromStore validates layout/version
  -> SettingsRepository loads working settings
  -> DeviceController applies settings
  -> hardware receives safe output state
  -> network services start
```

### Входящая команда

```text
Button / Web / MQTT
  -> validate and normalize
  -> same DeviceController API
  -> update working state/settings
  -> mark settings dirty when persistence is needed
  -> mark external state changed
  -> MQTT publishes actual resulting state
```

### Отложенное сохранение

```text
setting changed
  -> SettingsRepository marks dirty and records millis()
  -> new changes restart debounce interval
  -> tick() reaches quiet interval
  -> EepromStore writes complete consistent snapshot
  -> EEPROM.commit()
```

### Сетевой запуск

```text
WifiService opens setup AP and starts STA asynchronously
  -> loop keeps running
  -> STA connected
  -> setup AP closes
  -> OtaService begins OTA server
  -> MqttService connects and publishes discovery/state
  -> WebService remains available through STA address
```

## Правила зависимостей

Разрешённое направление:

```text
App/composition root
  -> network adapters
  -> core/domain controllers
  -> hardware drivers
  -> low-level libraries

core/domain controllers
  -> SettingsRepository
  -> hardware drivers

SettingsRepository
  -> EepromStore

EepromStore
  -> EEPROM library + layout/config structs
```

Network adapters могут вызывать публичный API core, но core не должен зависеть от Web UI, MQTT entity classes или HTTP field IDs.

Hardware не зависит от network. Storage не зависит от Web/MQTT. `eeprom_layout.h` может включать persisted data types, но не контроллеры и сервисы.

## Порядок переноса существующего проекта

Агент должен выполнять перестройку по этапам:

1. Зафиксировать текущее поведение и доступные PlatformIO environments.
2. Выделить предметное состояние и операции устройства.
3. Создать `config.h`, `core/`, `hardware/`, `storage/`, `network/`, `util/`.
4. Перенести GPIO и периферию в hardware-классы без изменения поведения.
5. Создать `EepromStore`, централизовать layout и убрать прямые EEPROM-вызовы из остальных классов.
6. Создать `SettingsRepository` для часто меняющихся настроек.
7. Создать единый прикладной controller API.
8. Создать корневой `App`, перенести туда владение и порядок жизненного цикла.
9. Сделать `main.cpp` тонким.
10. Перенести `WifiService` как неблокирующий AP/STA state machine.
11. Перенести обязательный `OtaService`, запускать его после STA connect.
12. Адаптировать `MqttService` к новому предметному API.
13. Адаптировать `WebService` к тому же API.
14. Добавить единый механизм отметки изменений состояния.
15. Разделить `platformio.ini`, `platformio.local.ini`, `platformio.local.example.ini`.
16. Обновить `.gitignore` и README.
17. Собрать один основной environment и устранить предупреждения/ошибки.

На каждом этапе сохранять рабочую сборку. Не смешивать архитектурный перенос с несвязанным изменением поведения устройства.

## Критерии готовности

Архитектура считается перенесённой, когда выполнены все условия:

- `main.cpp` только запускает корневой объект;
- корневой класс явно владеет всеми долгоживущими компонентами;
- статический dependency graph корневого класса выражен через brace NSDMI, fields объявлены в dependency order;
- зависимости передаются через конструкторы и не скрыты в глобальных объектах;
- `setup()` имеет понятный порядок;
- `loop()` неблокирующий и обслуживает каждый активный сервис;
- WiFi использует AP/STA state machine, timeout и reconnect без блокирующего ожидания;
- OTA обязателен в основном environment и начинается только после STA connect;
- MQTT и Web вызывают один прикладной API;
- MQTT публикует фактическое состояние устройства;
- EEPROM-адреса сосредоточены в одном versioned layout;
- изменение layout сопровождается defaults, проверками и политикой миграции;
- прямые EEPROM-вызовы отсутствуют вне `EepromStore`;
- часто меняющиеся настройки сохраняются отложенно через `SettingsRepository`;
- hardware-классы инкапсулируют GPIO и сторонние аппаратные библиотеки;
- `platformio.ini` содержит только общие данные;
- `platformio.local.ini` игнорируется Git;
- `platformio.local.example.ini` актуален и позволяет создать локальную конфигурацию;
- `.clang-format`, `.clang-tidy` и `.editorconfig` находятся в корне и согласованы между собой;
- private fields используют только suffix convention `camelCase_`;
- `Justfile` запускает build/compiledb/tidy для одного явно указанного environment;
- основной environment собирается отдельной командой без upload;
- предметные имена и поведение нового проекта не подменены ламповыми сущностями.

## Соглашения C++ и форматирование

Новый проект должен перенести не только архитектуру, но и единый C++ style contract. Форматирование и синтаксические соглашения уменьшают случайные различия между модулями и делают изменения разных агентов предсказуемыми.

### `.clang-format` — обязательный файл

Файл `.clang-format` из исходного проекта нужно копировать целиком в корень нового проекта. Не пересобирать его приблизительно по текстовому описанию. Конфигурация является исполняемой спецификацией стиля и рассчитана на clang-format 14+:

```yaml
---
# Formatting rules derived from the existing source style:
#   - 2-space indent, attached braces (1TBS), &/* aligned to type
#   - namespace body indented, access modifiers flush with `class`
# Targets clang-format 14+.

Language: Cpp
BasedOnStyle: LLVM

# --- indentation ---
IndentWidth: 2
TabWidth: 2
UseTab: Never
ColumnLimit: 120
ContinuationIndentWidth: 2

# --- braces: attached ---
BreakBeforeBraces: Custom
BraceWrapping:
  AfterClass: false
  AfterControlStatement: false
  AfterEnum: false
  AfterFunction: false
  AfterNamespace: false
  AfterStruct: false
  AfterUnion: false
  AfterExternBlock: false
  BeforeElse: false
  BeforeWhile: false
  IndentBraces: false
  SplitEmptyFunction: false
  SplitEmptyRecord: false
  SplitEmptyNamespace: false

# --- pointer / reference aligned to type ---
DerivePointerAlignment: false
PointerAlignment: Left

# --- class body ---
AccessModifierOffset: -2

# --- namespace body ---
NamespaceIndentation: All

# --- switch / case ---
IndentCaseLabels: true

# --- short constructs ---
AllowShortFunctionsOnASingleLine: InlineOnly
AllowShortIfStatementsOnASingleLine: Always
AllowShortLoopsOnASingleLine: false
AllowShortBlocksOnASingleLine: Never
AllowShortCaseLabelsOnASingleLine: true

# --- constructor initializer list ---
ConstructorInitializerIndentWidth: 2
PackConstructorInitializers: Never

# --- includes: preserve order ---
IncludeBlocks: Preserve
SortIncludes: Never

# --- alignment ---
AlignAfterOpenBracket: BlockIndent
AlignConsecutiveAssignments: false
AlignConsecutiveDeclarations: false
AlignEscapedNewlines: Left
AlignOperands: true
AlignTrailingComments: true
BinPackArguments: false
BinPackParameters: false
ReflowComments: false

# --- spacing ---
SpaceBeforeParens: ControlStatements
SpaceBeforeSquareBrackets: false
SpaceInEmptyParentheses: false
SpacesInContainerLiterals: false
```

Форматировать только изменённые C++-файлы:

```sh
clang-format -i src/core/app.cpp src/network/wifi_service.cpp
```

Проверка без изменения файлов:

```sh
clang-format --dry-run -Werror src/core/app.cpp src/network/wifi_service.cpp
```

Нельзя массово форматировать несвязанные файлы в рамках архитектурного переноса.

### `.editorconfig` — второй обязательный файл

`.editorconfig` также нужно копировать целиком. Он задаёт базовые правила редакторам и формат файлов, которые не покрывает clang-format:

```ini
# EditorConfig helps maintain consistent coding styles across editors
# See https://editorconfig.org for more information

root = true

[*]
charset = utf-8
end_of_line = lf
insert_final_newline = true
trim_trailing_whitespace = true

[*.{c,cc,cpp,cxx,h,hh,hpp,hxx,inc,ino,pde}]
indent_style = space
indent_size = 2

[*.{py,pyx,pyi}]
indent_style = space
indent_size = 4
max_line_length = 100

[*.{js,json}]
indent_style = space
indent_size = 2

[*.{yml,yaml}]
indent_style = space
indent_size = 2

[*.{json,jsonc}]
indent_style = space
indent_size = 2

[*.md]
trim_trailing_whitespace = false
max_line_length = off

[Makefile]
indent_style = tab

[*.mk]
indent_style = tab

[*.{sh,bash}]
indent_style = space
indent_size = 4

[*.{bat,cmd}]
end_of_line = crlf
```

### `.clang-tidy` — статический анализ

`.clang-tidy` нужно хранить в корне проекта рядом с `.clang-format`. Для ESP основной runner — PlatformIO static analysis: он передаёт clang-tidy defines и include paths выбранного environment без прямого использования несовместимых GCC compile flags.

Baseline включает ограниченный набор проверок. Это важно для embedded-проекта: включение всех `modernize-*`, `cppcoreguidelines-*` или `performance-*` сразу создаёт шум из Arduino framework и сторонних библиотек.

Универсальная конфигурация:

```yaml
---
Checks: >
  -*,
  clang-analyzer-core.*,
  clang-analyzer-deadcode.*,
  clang-analyzer-security.insecureAPI.strcpy,
  bugprone-branch-clone,
  bugprone-infinite-loop,
  bugprone-macro-parentheses,
  bugprone-misplaced-widening-cast,
  bugprone-sizeof-expression,
  bugprone-suspicious-memset-usage,
  bugprone-use-after-move,
  modernize-use-nullptr,
  readability-identifier-naming

WarningsAsErrors: ''
HeaderFilterRegex: '(^src/|.*/src/).*'
SystemHeaders: false
FormatStyle: file

CheckOptions:
  - key: readability-identifier-naming.PrivateMemberCase
    value: camelBack
  - key: readability-identifier-naming.PrivateMemberSuffix
    value: '_'
...
```

`readability-identifier-naming` закрепляет `camelCase_` для private fields. Если существующий проект ещё использует `_camelCase`, проверку можно временно добавить после механического переименования либо запускать до миграции без `WarningsAsErrors`. Целевое состояние документа и нового проекта — только suffix convention.

`HeaderFilterRegex` в переносимой версии не должен содержать `GyverLamp` или имя другого исходного проекта. Он ограничивает диагностику каталогом `src/` независимо от абсолютного пути workspace.

Сначала отдельно проверить сам ruleset установленным clang-tidy:

```sh
clang-tidy --verify-config
```

Основная проверка ESP environment:

```sh
pio check -e device1_ota --fail-on-defect=medium
```

Threshold `medium` включает medium и high defects. Не нужно одновременно передавать `--fail-on-defect=medium` и `--fail-on-defect=high`.

В `platformio.ini` это связывается с общим environment:

```ini
[env]
check_tool = clangtidy
check_flags =
  clangtidy: --config-file=.clang-tidy
```

Прямой вызов `clang-tidy -p .` по PlatformIO `compile_commands.json` не является основным ESP path. Host clang может не понимать Xtensa GCC flags и implicit toolchain configuration. Compilation database остаётся нужен clangd/LSP, а `pio check` — batch static analysis.

Headers отдельно передавать clang-tidy не нужно: они анализируются в контексте translation units, которые их включают. Диагностику системных headers не включать.

### Имена

- классы, структуры и enum-типы: `PascalCase`;
- методы и локальные переменные: `camelCase`;
- private fields: `camelCase_`;
- compile-time constants и macros: `UPPER_SNAKE_CASE`;
- файлы: `snake_case.h` и `snake_case.cpp`;
- пары файлов называются по основному классу: `wifi_service.h/.cpp`;
- feature flags: `USE_MQTT`, `USE_OTA`, `USE_BUTTON`.

Private fields всегда получают trailing underscore:

```cpp
class WifiService {
private:
  EepromStore& eeprom_;
  String deviceId_;
  bool connected_ = false;
};
```

Не использовать `_camelCase`. Идентификатор с одним leading underscore и lowercase внутри class scope обычно не зарезервирован стандартом C++, но правило легко пересекается с реально зарезервированными формами: `__name`, `_Name` во всех scopes и `_name` в global namespace. Suffix convention устраняет эту границу, лучше читается рядом с параметрами и одинаково проверяется clang-tidy.

```cpp
explicit WifiService(EepromStore& eeprom)
  : eeprom_(eeprom) {}
```

При совпадении parameter/member не добавлять искусственные сокращения: `eeprom` — параметр, `eeprom_` — поле.

Имена должны описывать ответственность. Не использовать универсальные `Manager`, `Helper` или `Utils`, если можно назвать точную роль: `WifiService`, `EepromStore`, `SettingsRepository`, `PeriodicTimer`.

### Заголовочные файлы

Использовать `#pragma once`. В header включать только типы, нужные для объявления. Для классов, хранимых по ссылке или указателю, предпочитать forward declarations:

```cpp
#pragma once

#include <Arduino.h>

class EepromStore;

class WifiService {
public:
  explicit WifiService(EepromStore& eeprom);

private:
  EepromStore& eeprom_;
};
```

Полное определение зависимости включается в `.cpp`. Не сортировать includes автоматически: PlatformIO/Arduino иногда чувствителен к их порядку. В `.cpp` первым обычно подключается собственный header, затем platform/library headers, затем локальные зависимости.

### Конструкторы и владение

Конструктор с одним логическим аргументом помечать `explicit`. Обязательные зависимости хранить как ссылки. Не использовать nullable pointer для зависимости, без которой объект не работает.

Default member initializer, или NSDMI, разрешён в любом классе, не только в composition root. Он может вызывать constructor поля с несколькими аргументами и использовать ранее объявленные fields:

```cpp
class DeviceRuntime {
public:
  explicit DeviceRuntime(EepromStore& eeprom)
    : eeprom_(eeprom) {}

private:
  EepromStore& eeprom_;
  SettingsRepository settings_{eeprom_};
  MotorDriver motors_;
  MotionController motion_{motors_, settings_};
};
```

Здесь порядок такой:

1. constructor parameter `eeprom` привязывается к reference field `eeprom_` через constructor initializer list;
2. `settings_` создаётся из уже привязанного `eeprom_`;
3. `motors_` default-constructs;
4. `motion_` получает уже созданные `motors_` и `settings_`.

Constructor parameter нельзя напрямую использовать в NSDMI, потому что initializer записан вне конкретного constructor scope. Сначала сохранить parameter в ранее объявленное поле либо инициализировать зависимый member в constructor initializer list.

NSDMI используется, когда:

- значение одинаково для всех constructors;
- owned subobject зависит только от ранее объявленных members и compile-time constants;
- composition graph статичен;
- initializer короткий и ясно показывает связь.

Constructor initializer list использовать, когда:

- значение приходит напрямую из constructor parameter;
- разные constructors создают member по-разному;
- инициализация требует validation или предварительного вычисления;
- NSDMI скроет важную вариативность объекта.

Если constructor явно инициализирует member в initializer list, его NSDMI для этого constructor игнорируется:

```cpp
class TimerOwner {
public:
  TimerOwner() = default;
  explicit TimerOwner(uint32_t intervalMs)
    : timer_(intervalMs) {}

private:
  Timer timer_{1000};
};
```

Нельзя ссылаться на позже объявленный member:

```cpp
class InvalidOrder {
private:
  MotionController motion_{motors_}; // motors_ ещё не сконструирован
  MotorDriver motors_;
};
```

Такой код может компилироваться, но передаёт ссылку на объект до начала его lifetime. Declaration order нужно проверять как dependency order.

Для class-type field запись `MotorDriver motors_;` вызывает default constructor. Для scalar field она оставляет неопределённое значение. Scalars всегда инициализировать явно:

```cpp
bool connected_{false};
uint32_t startedAt_{};
uint8_t retryCount_{0};
```

Brace initialization предпочтительна: единый синтаксис и compile-time запрет narrowing conversions.

```cpp
explicit SettingsRepository(EepromStore& eeprom)
  : eeprom_(eeprom) {}
```

Initializer list оформлять по одному member на строку. Порядок initializer list должен совпадать с порядком объявления полей.

Значения простых полей задавать при объявлении:

```cpp
bool dirty_ = false;
uint32_t changedAt_ = 0;
DeviceMode mode_ = DeviceMode::Off;
```

Не использовать owning raw pointers. Для статического графа ESP-проекта объекты принадлежат `App`, а сервисы получают ссылки.

### Типы и преобразования

Использовать целочисленные типы фиксированной ширины для GPIO, протоколов, persisted data и таймеров:

```cpp
uint8_t channel;
uint16_t intervalSec;
uint32_t startedAt;
```

Обычный `int` допустим для API, где именно `int` является естественным типом: адрес EEPROM, результат `analogRead()`, индекс сторонней библиотеки.

Для преобразований использовать C++ casts. Основной вариант — `static_cast`:

```cpp
const uint8_t rawMode = static_cast<uint8_t>(DeviceMode::Automatic);
const DeviceMode mode = static_cast<DeviceMode>(rawMode);
const uint32_t durationMs = static_cast<uint32_t>(durationSec) * 1000UL;
```

Не использовать C-style cast:

```cpp
// Не делать
uint8_t value = (uint8_t)input;
```

`reinterpret_cast` разрешён только на реальной ABI-границе или при работе с byte buffer сторонней библиотеки. Он не должен маскировать несовместимую модель данных. `const_cast` и `dynamic_cast` не использовать без отдельного обоснования.

Перед narrowing cast значение нужно проверить или ограничить:

```cpp
const uint16_t bounded = constrain(input, 0, 255);
const uint8_t value = static_cast<uint8_t>(bounded);
```

### Enum

Использовать scoped enums с явным базовым типом, особенно для EEPROM и протоколов:

```cpp
enum class DeviceMode : uint8_t {
  Off,
  Manual,
  Automatic,
};
```

Перед преобразованием байта из EEPROM валидировать диапазон. Сам `static_cast<DeviceMode>(raw)` не делает значение допустимым.

### `const`, `constexpr`, `nullptr`

- неизменяемые локальные значения объявлять `const`;
- compile-time constants объявлять `constexpr`;
- методы чтения помечать `const`;
- параметры без копирования передавать как `const T&`;
- нулевой указатель писать как `nullptr`, не `NULL` и не `0`.

```cpp
static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;

bool isConnected() const;
const DeviceSettings& settings() const;
```

### Управляющий поток

Предпочитать ранние выходы и guard clauses. Они уменьшают вложенность в `tick()`:

```cpp
void OtaService::tick() {
  if (!wifi_.isStaConnected()) return;
  if (!begun_) begin();
  ArduinoOTA.handle();
}
```

Короткий однострочный `if` допустим по `.clang-format`. Сложное условие или действие оформлять блоком.

Для `switch` по `enum class` обрабатывать все состояния. `default` можно не писать, если compiler warning должен обнаружить новое необработанное значение. Для внешнего числового input сначала выполнять validation.

### Время и переполнение `millis()`

Сравнивать интервалы через unsigned subtraction:

```cpp
if (millis() - startedAt_ < TIMEOUT_MS) return;
```

Не сравнивать абсолютные моменты через `millis() >= startedAt_ + TIMEOUT_MS`: такой код ломается около переполнения счётчика.

### Строки и буферы

Для persisted credentials и протокольных payload использовать фиксированные `char` buffers. Копировать через `strlcpy`, форматировать через `snprintf`:

```cpp
char host[MQTT_HOST_LEN] = {};
strlcpy(config.host, host, sizeof(config.host));
snprintf(topic, sizeof(topic), "%s/state", deviceId);
```

Проверять truncation, если потеря данных меняет смысл. Не использовать `strcpy` и `sprintf` с внешними данными.

Arduino `String` допустим для короткоживущих UI-строк, построения option lists и API библиотек. Не хранить объект `String` как сырые EEPROM bytes. В часто вызываемом коде избегать постоянной конкатенации, создающей heap fragmentation.

Для постоянных Serial literals на ESP8266 предпочтителен `F()`:

```cpp
Serial.println(F("[WIFI] Connected"));
```

### Условная компиляция

`#ifdef USE_*` локализовать внутри соответствующего header/implementation. Внешний API класса должен сохраняться при выключенной возможности.

Если no-op constructor получает неиспользуемые параметры, явно погасить warnings:

```cpp
(void)eeprom;
(void)wifi;
```

Не распространять feature `#ifdef` по `App`, Web, storage и предметным контроллерам.

### Проверки времени компиляции

Использовать `static_assert` для архитектурных invariants, известных компилятору:

- размер persisted-структуры;
- capacity таблицы;
- отсутствие пересечения EEPROM blocks;
- попадание последнего блока в `EEPROM_SIZE`;
- размер статического storage buffer;
- соответствие количества enum entries таблице.

Compile-time проверка предпочтительнее комментария, который может устареть.

### Ошибки и возвращаемые значения

Операции записи и запуска, способные завершиться ошибкой, возвращают `bool` или явный status enum. Результат нельзя молча игнорировать в critical path.

Исключения и RTTI не вводить без необходимости: для типичного Arduino firmware важнее предсказуемые flash/RAM costs и явные status values.

## Эталонные реализации

Следующие приложения задают переносимый baseline. WiFi, OTA и EEPROM layout следует переносить максимально близко. MQTT и Web представлены infrastructure-каркасами: их transport lifecycle сохраняется, а entities, поля формы и команды заменяются предметными типами нового устройства.

### Приложение A. Полный `WifiService`

Baseline ниже рассчитан на ESP8266. Для ESP32 меняются WiFi include и отдельные platform calls, но состояния и жизненный цикл сохраняются.

Требуемые параметры в `config.h`:

```cpp
#pragma once

#ifndef DEVICE_NAME
#define DEVICE_NAME "EspDevice"
#endif

#define AP_SSID DEVICE_NAME
#define AP_PASS "12345678"
#define AP_IP   {192, 168, 4, 1}
```

`network/wifi_config.h`:

```cpp
#pragma once

#include <Arduino.h>

constexpr uint8_t WIFI_SSID_LEN = 33;
constexpr uint8_t WIFI_PASS_LEN = 65;

struct WifiConfig {
  char ssid[WIFI_SSID_LEN];
  char password[WIFI_PASS_LEN];
};

static_assert(sizeof(WifiConfig) == WIFI_SSID_LEN + WIFI_PASS_LEN, "Unexpected WifiConfig padding");
```

`network/wifi_service.h`:

```cpp
#pragma once

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "../config.h"

class EepromStore;

class WifiService {
public:
  explicit WifiService(EepromStore& eeprom)
    : eeprom_(eeprom),
      deviceId_(DEVICE_NAME) {}

  void init();
  void tick();

  bool isStaConnected() const { return WiFi.isConnected(); }
  const String& getDeviceId() const { return deviceId_; }

private:
  enum class StaState : uint8_t {
    Provisioning,
    Connecting,
    Connected,
    RetryWait,
  };

  enum class ApState : uint8_t {
    Inactive,
    Active,
    RetryWait,
  };

  EepromStore& eeprom_;
  String deviceId_;

  StaState staState_ = StaState::Provisioning;
  ApState apState_ = ApState::Inactive;
  uint32_t apStartedAt_ = 0;
  uint32_t apRetryStartedAt_ = 0;
  uint32_t connectStartedAt_ = 0;
  uint32_t retryStartedAt_ = 0;

  static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;
  static constexpr uint32_t AP_RETRY_INTERVAL_MS = 5000;
  static constexpr uint32_t AP_TIMEOUT_MS = 5UL * 60UL * 1000UL;
  static constexpr uint32_t STA_CONNECT_TIMEOUT_MS = 10000;

  bool startAp();
  void requestAp();
  void stopAp();
  void startStaConnection();
  void checkStaConnecting();
  void checkStaRetryWait();
  void onStaConnected();
  void checkApRetry();
  void checkApTimeout();
};
```

`network/wifi_service.cpp`:

```cpp
#include "wifi_service.h"

#include "../storage/eeprom_store.h"

void WifiService::init() {
  const WifiConfig& wifiConfig = eeprom_.readWifiConfig();

  if (strlen(wifiConfig.ssid) == 0) {
    WiFi.mode(WIFI_AP);
    requestAp();
    staState_ = StaState::Provisioning;
    Serial.println(F("[WIFI] No STA config, AP open for setup"));
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  requestAp();
  startStaConnection();
}

void WifiService::tick() {
  switch (staState_) {
    case StaState::Provisioning: checkApTimeout(); break;

    case StaState::Connecting: checkStaConnecting(); break;

    case StaState::Connected:
      if (!isStaConnected()) {
        staState_ = StaState::RetryWait;
        retryStartedAt_ = millis();
        Serial.println(F("[WIFI] STA connection lost"));
      }
      break;

    case StaState::RetryWait:
      checkStaRetryWait();
      checkApTimeout();
      break;
  }

  if (staState_ != StaState::Connected && !isStaConnected()) {
    checkApRetry();
  }
}

bool WifiService::startAp() {
  if (apState_ == ApState::Active) return true;

  const uint8_t ipBytes[4] = AP_IP;
  const IPAddress apIp(ipBytes[0], ipBytes[1], ipBytes[2], ipBytes[3]);

  if (!WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0))) {
    Serial.println(F("[WIFI] Failed to configure AP"));
    return false;
  }

  if (!WiFi.softAP(AP_SSID, AP_PASS)) {
    Serial.println(F("[WIFI] Failed to start AP"));
    return false;
  }

  apState_ = ApState::Active;
  apStartedAt_ = millis();

  Serial.print(F("[WIFI] AP IP: "));
  Serial.println(WiFi.softAPIP());
  return true;
}

void WifiService::requestAp() {
  if (startAp()) return;

  apState_ = ApState::RetryWait;
  apRetryStartedAt_ = millis();
}

void WifiService::stopAp() {
  if (apState_ == ApState::Inactive) return;

  WiFi.softAPdisconnect(true);
  apState_ = ApState::Inactive;

  const WifiConfig& wifiConfig = eeprom_.readWifiConfig();
  WiFi.mode(strlen(wifiConfig.ssid) == 0 ? WIFI_OFF : WIFI_STA);
}

void WifiService::startStaConnection() {
  const WifiConfig& wifiConfig = eeprom_.readWifiConfig();
  if (strlen(wifiConfig.ssid) == 0) {
    staState_ = StaState::Provisioning;
    return;
  }

  Serial.print(F("[WIFI] Connecting to "));
  Serial.println(wifiConfig.ssid);

  WiFi.begin(wifiConfig.ssid, wifiConfig.password);
  connectStartedAt_ = millis();
  staState_ = StaState::Connecting;
}

void WifiService::checkStaConnecting() {
  if (isStaConnected()) {
    onStaConnected();
    return;
  }

  if (millis() - connectStartedAt_ < STA_CONNECT_TIMEOUT_MS) return;

  staState_ = StaState::RetryWait;
  retryStartedAt_ = millis();
  Serial.println(F("[WIFI] STA connection failed, keeping AP for setup"));
}

void WifiService::checkStaRetryWait() {
  if (isStaConnected()) {
    onStaConnected();
    return;
  }

  if (millis() - retryStartedAt_ < RECONNECT_INTERVAL_MS) return;
  startStaConnection();
}

void WifiService::onStaConnected() {
  staState_ = StaState::Connected;

  Serial.print(F("[WIFI] STA IP: "));
  Serial.println(WiFi.localIP());

  stopAp();
}

void WifiService::checkApRetry() {
  if (apState_ != ApState::RetryWait) return;
  if (millis() - apRetryStartedAt_ < AP_RETRY_INTERVAL_MS) return;

  requestAp();
}

void WifiService::checkApTimeout() {
  if (apState_ != ApState::Active) return;
  if (millis() - apStartedAt_ < AP_TIMEOUT_MS) return;

  if (WiFi.softAPgetStationNum() > 0) {
    apStartedAt_ = millis();
    return;
  }

  Serial.println(F("[WIFI] AP setup timeout"));
  stopAp();
}
```

После сохранения новых WiFi credentials простой baseline предполагает controlled restart. Если нужен reconnect без перезагрузки, добавить публичный `reloadConfig()` внутри `WifiService`; WebService не должен самостоятельно вызывать `WiFi.begin()`.

Для ESP32 заменить `<ESP8266WiFi.h>` на `<WiFi.h>`, проверить сигнатуры `softAPdisconnect()` и доступность `WIFI_OFF`. State machine и EEPROM API не менять.

### Приложение B. Полный `OtaService`

`network/ota_service.h`:

```cpp
#pragma once

class WifiService;

class OtaService {
public:
  explicit OtaService(WifiService& wifi)
    : wifi_(wifi) {}

  void init();
  void tick();

private:
  WifiService& wifi_;
  bool begun_ = false;
};
```

`network/ota_service.cpp`:

```cpp
#include "ota_service.h"

#ifdef USE_OTA

#include <Arduino.h>
#include <ArduinoOTA.h>

#include "wifi_service.h"

void OtaService::init() {
  ArduinoOTA.setHostname(wifi_.getDeviceId().c_str());

  ArduinoOTA.onStart([]() {
    Serial.println(F("[OTA] Start"));
    // Перевести выходы устройства в безопасное состояние, если требуется.
  });

  ArduinoOTA.onEnd([]() {
    Serial.println();
    Serial.println(F("[OTA] End"));
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total == 0) return;

    const unsigned int percent = static_cast<unsigned int>(
      static_cast<uint64_t>(progress) * 100ULL / static_cast<uint64_t>(total)
    );
    Serial.printf("[OTA] Progress: %u%%\r", percent);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error %u: ", static_cast<unsigned int>(error));

    switch (error) {
      case OTA_AUTH_ERROR: Serial.println(F("Auth failed")); break;
      case OTA_BEGIN_ERROR: Serial.println(F("Begin failed")); break;
      case OTA_CONNECT_ERROR: Serial.println(F("Connect failed")); break;
      case OTA_RECEIVE_ERROR: Serial.println(F("Receive failed")); break;
      case OTA_END_ERROR: Serial.println(F("End failed")); break;
      default: Serial.println(F("Unknown error")); break;
    }
  });
}

void OtaService::tick() {
  if (!begun_) {
    if (!wifi_.isStaConnected()) return;

    ArduinoOTA.begin();
    begun_ = true;
    Serial.println(F("[OTA] Ready"));
  }

  ArduinoOTA.handle();
}

#else

void OtaService::init() {
}

void OtaService::tick() {
}

#endif
```

Если основной environment обязан поддерживать OTA, в его build flags должен присутствовать `-D USE_OTA`. No-op ветка остаётся полезной для специализированных USB/debug builds.

### Приложение C. Полный пример EEPROM layout

Пример использует отдельный persisted DTO. Runtime model может содержать `bool` и `enum class`, но на flash записываются байты с гарантированным размером.

`core/device_settings.h`:

```cpp
#pragma once

#include <Arduino.h>

enum class DeviceMode : uint8_t {
  Off,
  Manual,
  Automatic,
  Count,
};

struct PersistedDeviceSettings {
  uint8_t powerOn = 0;
  uint8_t targetValue = 0;
  uint8_t mode = static_cast<uint8_t>(DeviceMode::Off);
};

static_assert(sizeof(PersistedDeviceSettings) == 3, "Unexpected PersistedDeviceSettings size");
```

`network/mqtt_config.h`:

```cpp
#pragma once

#include <Arduino.h>

constexpr uint8_t MQTT_HOST_LEN = 33;
constexpr uint8_t MQTT_PORT_LEN = 10;
constexpr uint8_t MQTT_USER_LEN = 33;
constexpr uint8_t MQTT_PASS_LEN = 65;

struct MqttConfig {
  char host[MQTT_HOST_LEN];
  char port[MQTT_PORT_LEN];
  char user[MQTT_USER_LEN];
  char password[MQTT_PASS_LEN];
};

static_assert(
  sizeof(MqttConfig) == MQTT_HOST_LEN + MQTT_PORT_LEN + MQTT_USER_LEN + MQTT_PASS_LEN,
  "Unexpected MqttConfig padding"
);
```

`storage/eeprom_layout.h`:

```cpp
#pragma once

#include <Arduino.h>

#include "../core/device_settings.h"
#include "../network/mqtt_config.h"
#include "../network/wifi_config.h"

constexpr int EEPROM_SIZE = 512;

constexpr uint32_t EEPROM_LAYOUT_MAGIC = 0x45535031; // "ESP1"
constexpr uint8_t EEPROM_LAYOUT_VERSION_CURRENT = 1;

constexpr int EEPROM_LAYOUT_META_ADDR = 0;
constexpr int EEPROM_LAYOUT_META_SIZE = 8;
constexpr int EEPROM_LAYOUT_MAGIC_ADDR = EEPROM_LAYOUT_META_ADDR;
constexpr int EEPROM_LAYOUT_VERSION_ADDR = EEPROM_LAYOUT_META_ADDR + sizeof(EEPROM_LAYOUT_MAGIC);

constexpr int EEPROM_RESERVED_TOP_ADDR = EEPROM_LAYOUT_META_ADDR + EEPROM_LAYOUT_META_SIZE; // 8
constexpr int EEPROM_RESERVED_TOP_SIZE = 8;

constexpr int EEPROM_WIFI_BLOCK_ADDR = EEPROM_RESERVED_TOP_ADDR + EEPROM_RESERVED_TOP_SIZE; // 16
constexpr int EEPROM_WIFI_BLOCK_SIZE = 112;
constexpr int EEPROM_WIFI_CONFIG_ADDR = EEPROM_WIFI_BLOCK_ADDR;
constexpr int EEPROM_WIFI_CONFIG_SIZE = sizeof(WifiConfig);

constexpr int EEPROM_MQTT_BLOCK_ADDR = EEPROM_WIFI_BLOCK_ADDR + EEPROM_WIFI_BLOCK_SIZE; // 128
constexpr int EEPROM_MQTT_BLOCK_SIZE = 144;
constexpr int EEPROM_MQTT_CONFIG_ADDR = EEPROM_MQTT_BLOCK_ADDR;
constexpr int EEPROM_MQTT_CONFIG_SIZE = sizeof(MqttConfig);

constexpr int EEPROM_DEVICE_BLOCK_ADDR = EEPROM_MQTT_BLOCK_ADDR + EEPROM_MQTT_BLOCK_SIZE; // 272
constexpr int EEPROM_DEVICE_BLOCK_SIZE = 64;
constexpr int EEPROM_DEVICE_SETTINGS_ADDR = EEPROM_DEVICE_BLOCK_ADDR;
constexpr int EEPROM_DEVICE_SETTINGS_SIZE = sizeof(PersistedDeviceSettings);

constexpr int EEPROM_RESERVED_BOTTOM_ADDR = EEPROM_DEVICE_BLOCK_ADDR + EEPROM_DEVICE_BLOCK_SIZE; // 336
constexpr int EEPROM_RESERVED_BOTTOM_SIZE = EEPROM_SIZE - EEPROM_RESERVED_BOTTOM_ADDR;

static_assert(
  EEPROM_LAYOUT_META_ADDR + EEPROM_LAYOUT_META_SIZE <= EEPROM_RESERVED_TOP_ADDR,
  "EEPROM metadata overlaps top reserve"
);
static_assert(
  EEPROM_RESERVED_TOP_ADDR + EEPROM_RESERVED_TOP_SIZE <= EEPROM_WIFI_BLOCK_ADDR,
  "EEPROM top reserve overlaps WiFi block"
);
static_assert(EEPROM_WIFI_CONFIG_SIZE <= EEPROM_WIFI_BLOCK_SIZE, "EEPROM WiFi config does not fit its block");
static_assert(
  EEPROM_WIFI_BLOCK_ADDR + EEPROM_WIFI_BLOCK_SIZE <= EEPROM_MQTT_BLOCK_ADDR,
  "EEPROM WiFi block overlaps MQTT block"
);
static_assert(EEPROM_MQTT_CONFIG_SIZE <= EEPROM_MQTT_BLOCK_SIZE, "EEPROM MQTT config does not fit its block");
static_assert(
  EEPROM_MQTT_BLOCK_ADDR + EEPROM_MQTT_BLOCK_SIZE <= EEPROM_DEVICE_BLOCK_ADDR,
  "EEPROM MQTT block overlaps device block"
);
static_assert(
  EEPROM_DEVICE_SETTINGS_SIZE <= EEPROM_DEVICE_BLOCK_SIZE,
  "EEPROM device settings do not fit their block"
);
static_assert(
  EEPROM_DEVICE_BLOCK_ADDR + EEPROM_DEVICE_BLOCK_SIZE <= EEPROM_RESERVED_BOTTOM_ADDR,
  "EEPROM device block overlaps bottom reserve"
);
static_assert(EEPROM_RESERVED_BOTTOM_SIZE >= 0, "EEPROM layout exceeds EEPROM_SIZE");
static_assert(
  EEPROM_RESERVED_BOTTOM_ADDR + EEPROM_RESERVED_BOTTOM_SIZE == EEPROM_SIZE,
  "EEPROM layout must end exactly at EEPROM_SIZE"
);
```

Размеры блоков намеренно больше payload. Новый layout сначала расходует reserve внутри существующего блока. Адрес следующего блока не сдвигается. Это уменьшает число разрушительных миграций.

`storage/eeprom_store.h`:

```cpp
#pragma once

#include "../core/device_settings.h"
#include "../network/mqtt_config.h"
#include "../network/wifi_config.h"

class EepromStore {
public:
  void init();

  WifiConfig readWifiConfig() const;
  bool writeWifiConfig(const char* ssid, const char* password);

  MqttConfig readMqttConfig() const;
  bool writeMqttConfig(const char* host, const char* port, const char* user, const char* password);

  PersistedDeviceSettings readDeviceSettings() const;
  bool writeDeviceSettings(const PersistedDeviceSettings& settings);

private:
  bool hasCurrentLayout() const;
  bool initializeLayout();
};
```

`storage/eeprom_store.cpp`:

```cpp
#include "eeprom_store.h"

#include <Arduino.h>
#include <EEPROM.h>

#include "eeprom_layout.h"

void EepromStore::init() {
  EEPROM.begin(EEPROM_SIZE);

  if (hasCurrentLayout()) return;

  // При появлении version 2 здесь должен быть switch по старой версии и
  // отдельная миграция, читающая старые адреса. Не читать старый layout
  // константами новой версии.
  if (!initializeLayout()) {
    Serial.println(F("[EEPROM] Layout initialization failed"));
  }
}

bool EepromStore::hasCurrentLayout() const {
  uint32_t magic = 0;
  EEPROM.get(EEPROM_LAYOUT_MAGIC_ADDR, magic);

  const uint8_t version = EEPROM.read(EEPROM_LAYOUT_VERSION_ADDR);
  return magic == EEPROM_LAYOUT_MAGIC && version == EEPROM_LAYOUT_VERSION_CURRENT;
}

bool EepromStore::initializeLayout() {
  for (int address = 0; address < EEPROM_SIZE; ++address) {
    EEPROM.write(address, 0);
  }

  const PersistedDeviceSettings defaults{};
  EEPROM.put(EEPROM_DEVICE_SETTINGS_ADDR, defaults);
  EEPROM.put(EEPROM_LAYOUT_MAGIC_ADDR, EEPROM_LAYOUT_MAGIC);
  EEPROM.write(EEPROM_LAYOUT_VERSION_ADDR, EEPROM_LAYOUT_VERSION_CURRENT);
  return EEPROM.commit();
}

WifiConfig EepromStore::readWifiConfig() const {
  WifiConfig config{};
  EEPROM.get(EEPROM_WIFI_CONFIG_ADDR, config);
  config.ssid[WIFI_SSID_LEN - 1] = '\0';
  config.password[WIFI_PASS_LEN - 1] = '\0';
  return config;
}

bool EepromStore::writeWifiConfig(const char* ssid, const char* password) {
  WifiConfig config{};
  strlcpy(config.ssid, ssid, sizeof(config.ssid));
  strlcpy(config.password, password, sizeof(config.password));

  EEPROM.put(EEPROM_WIFI_CONFIG_ADDR, config);
  return EEPROM.commit();
}

MqttConfig EepromStore::readMqttConfig() const {
  MqttConfig config{};
  EEPROM.get(EEPROM_MQTT_CONFIG_ADDR, config);
  config.host[MQTT_HOST_LEN - 1] = '\0';
  config.port[MQTT_PORT_LEN - 1] = '\0';
  config.user[MQTT_USER_LEN - 1] = '\0';
  config.password[MQTT_PASS_LEN - 1] = '\0';
  return config;
}

bool EepromStore::writeMqttConfig(
  const char* host,
  const char* port,
  const char* user,
  const char* password
) {
  MqttConfig config{};
  strlcpy(config.host, host, sizeof(config.host));
  strlcpy(config.port, port, sizeof(config.port));
  strlcpy(config.user, user, sizeof(config.user));
  strlcpy(config.password, password, sizeof(config.password));

  EEPROM.put(EEPROM_MQTT_CONFIG_ADDR, config);
  return EEPROM.commit();
}

PersistedDeviceSettings EepromStore::readDeviceSettings() const {
  PersistedDeviceSettings settings{};
  EEPROM.get(EEPROM_DEVICE_SETTINGS_ADDR, settings);

  if (settings.powerOn > 1) settings.powerOn = 0;
  if (settings.mode >= static_cast<uint8_t>(DeviceMode::Count)) {
    settings.mode = static_cast<uint8_t>(DeviceMode::Off);
  }

  return settings;
}

bool EepromStore::writeDeviceSettings(const PersistedDeviceSettings& settings) {
  EEPROM.put(EEPROM_DEVICE_SETTINGS_ADDR, settings);
  return EEPROM.commit();
}
```

При следующей версии нельзя ограничиться увеличением `EEPROM_LAYOUT_VERSION_CURRENT`. Нужно добавить старые адреса в отдельный namespace, прочитать совместимые поля до очистки, записать defaults новой версии и восстановить проверенные значения.

### Приложение D. Полный `SettingsRepository`

`storage/settings_repository.h`:

```cpp
#pragma once

#include <Arduino.h>

#include "../core/device_settings.h"

class EepromStore;

class SettingsRepository {
public:
  explicit SettingsRepository(EepromStore& eeprom)
    : eeprom_(eeprom) {}

  void init();
  void tick();

  bool isPowerOn() const { return settings_.powerOn != 0; }
  uint8_t getTargetValue() const { return settings_.targetValue; }
  DeviceMode getMode() const { return static_cast<DeviceMode>(settings_.mode); }

  void setPowerOn(bool powerOn);
  void setTargetValue(uint8_t value);
  void setMode(DeviceMode mode);

  bool saveNow();

private:
  static constexpr uint32_t SAVE_DELAY_MS = 30000;

  EepromStore& eeprom_;
  PersistedDeviceSettings settings_{};
  bool dirty_ = false;
  uint32_t changedAt_ = 0;

  void markDirty();
};
```

`storage/settings_repository.cpp`:

```cpp
#include "settings_repository.h"

#include "eeprom_store.h"

void SettingsRepository::init() {
  settings_ = eeprom_.readDeviceSettings();
  dirty_ = false;
}

void SettingsRepository::tick() {
  if (!dirty_) return;
  if (millis() - changedAt_ < SAVE_DELAY_MS) return;

  saveNow();
}

void SettingsRepository::setPowerOn(bool powerOn) {
  const uint8_t value = powerOn ? 1 : 0;
  if (settings_.powerOn == value) return;

  settings_.powerOn = value;
  markDirty();
}

void SettingsRepository::setTargetValue(uint8_t value) {
  if (settings_.targetValue == value) return;

  settings_.targetValue = value;
  markDirty();
}

void SettingsRepository::setMode(DeviceMode mode) {
  if (mode >= DeviceMode::Count) return;

  const uint8_t value = static_cast<uint8_t>(mode);
  if (settings_.mode == value) return;

  settings_.mode = value;
  markDirty();
}

bool SettingsRepository::saveNow() {
  if (!dirty_) return true;

  if (!eeprom_.writeDeviceSettings(settings_)) {
    Serial.println(F("[SETTINGS] Save failed"));
    changedAt_ = millis();
    return false;
  }

  dirty_ = false;
  return true;
}

void SettingsRepository::markDirty() {
  dirty_ = true;
  changedAt_ = millis();
}
```

Для нескольких независимых persisted-блоков репозиторий может иметь отдельные dirty flags. Это позволяет не перезаписывать большой блок из-за изменения одного несвязанного параметра.

### Приложение E. Infrastructure-каркас `MqttService`

Этот пример использует обычный `PubSubClient` и одну power-команду. Если проект использует Home Assistant entity library, сохранить lifecycle, reconnect и направление данных, а entity registration заменить API выбранной библиотеки.

Ожидаемый предметный API:

```cpp
class DeviceController {
public:
  void setPower(bool enabled);
  bool isPowerOn() const;
};
```

`network/mqtt_service.h`:

```cpp
#pragma once

#include <Arduino.h>

class DeviceController;
class EepromStore;
class WifiService;

#ifdef USE_MQTT
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "mqtt_config.h"
#endif

class MqttService {
public:
  MqttService(EepromStore& eeprom, DeviceController& device, WifiService& wifi);

  void init();
  void tick();
  void reloadConfig();
  void updateStates();

private:
#ifdef USE_MQTT
  static constexpr uint32_t RECONNECT_INTERVAL_MS = 5000;
  static constexpr uint16_t DEFAULT_PORT = 1883;

  EepromStore& eeprom_;
  DeviceController& device_;
  WifiService& wifi_;

  WiFiClient wifiClient_;
  PubSubClient client_;
  MqttConfig config_{};

  bool stateDirty_ = true;
  bool connectAttempted_ = false;
  uint32_t lastConnectAttemptAt_ = 0;

  bool connect();
  void handleMessage(char* topic, uint8_t* payload, unsigned int length);
  void publishStates();
  uint16_t parsePort() const;
  void makeTopic(char* output, size_t outputSize, const char* suffix) const;
#endif
};
```

`network/mqtt_service.cpp`:

```cpp
#include "mqtt_service.h"

#ifdef USE_MQTT

#include <stdlib.h>
#include <string.h>

#include "../core/device_controller.h"
#include "../storage/eeprom_store.h"
#include "wifi_service.h"

MqttService::MqttService(EepromStore& eeprom, DeviceController& device, WifiService& wifi)
  : eeprom_(eeprom),
    device_(device),
    wifi_(wifi),
    wifiClient_(),
    client_(wifiClient_) {}

void MqttService::init() {
  client_.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
    handleMessage(topic, payload, length);
  });
  reloadConfig();
}

void MqttService::tick() {
  if (strlen(config_.host) == 0 || !wifi_.isStaConnected()) {
    if (client_.connected()) client_.disconnect();
    return;
  }

  if (!client_.connected()) {
    const uint32_t now = millis();
    if (connectAttempted_ && now - lastConnectAttemptAt_ < RECONNECT_INTERVAL_MS) return;

    connectAttempted_ = true;
    lastConnectAttemptAt_ = now;
    if (!connect()) return;
  }

  client_.loop();
  if (stateDirty_) publishStates();
}

void MqttService::reloadConfig() {
  if (client_.connected()) client_.disconnect();

  config_ = eeprom_.readMqttConfig();
  client_.setServer(config_.host, parsePort());
  connectAttempted_ = false;
  stateDirty_ = true;
}

void MqttService::updateStates() {
  stateDirty_ = true;
}

bool MqttService::connect() {
  const char* user = strlen(config_.user) == 0 ? nullptr : config_.user;
  const char* password = user == nullptr ? nullptr : config_.password;

  Serial.print(F("[MQTT] Connecting to "));
  Serial.println(config_.host);

  if (!client_.connect(wifi_.getDeviceId().c_str(), user, password)) {
    Serial.print(F("[MQTT] Connect failed, state="));
    Serial.println(client_.state());
    return false;
  }

  char topic[96];
  makeTopic(topic, sizeof(topic), "power/set");
  client_.subscribe(topic);

  stateDirty_ = true;
  Serial.println(F("[MQTT] Connected"));
  return true;
}

void MqttService::handleMessage(char* topic, uint8_t* payload, unsigned int length) {
  char powerTopic[96];
  makeTopic(powerTopic, sizeof(powerTopic), "power/set");
  if (strcmp(topic, powerTopic) != 0) return;

  const bool turnOn = length == 2 && payload[0] == 'O' && payload[1] == 'N';
  const bool turnOff = length == 3 && payload[0] == 'O' && payload[1] == 'F' && payload[2] == 'F';
  if (!turnOn && !turnOff) return;

  device_.setPower(turnOn);
  updateStates();
}

void MqttService::publishStates() {
  if (!client_.connected()) return;

  char topic[96];
  makeTopic(topic, sizeof(topic), "power/state");

  if (client_.publish(topic, device_.isPowerOn() ? "ON" : "OFF", true)) {
    stateDirty_ = false;
  }
}

uint16_t MqttService::parsePort() const {
  if (strlen(config_.port) == 0) return DEFAULT_PORT;

  char* end = nullptr;
  const unsigned long value = strtoul(config_.port, &end, 10);
  if (end == config_.port || *end != '\0' || value == 0 || value > 65535UL) {
    return DEFAULT_PORT;
  }

  return static_cast<uint16_t>(value);
}

void MqttService::makeTopic(char* output, size_t outputSize, const char* suffix) const {
  snprintf(output, outputSize, "%s/%s", wifi_.getDeviceId().c_str(), suffix);
}

#else

MqttService::MqttService(EepromStore& eeprom, DeviceController& device, WifiService& wifi) {
  (void)eeprom;
  (void)device;
  (void)wifi;
}

void MqttService::init() {
}

void MqttService::tick() {
}

void MqttService::reloadConfig() {
}

void MqttService::updateStates() {
}

#endif
```

`PubSubClient::connect()` может блокироваться на TCP timeout. Каркас не вызывает его в tight loop, но для строгих latency-требований нужно выбрать async MQTT library или уменьшить transport timeout. Нельзя делать дополнительный собственный цикл повторных попыток внутри `connect()`.

Для Home Assistant добавить device metadata, availability, discovery entities и восстановление discovery после reconnect. Каждая command callback должна вызывать `DeviceController`, после чего `updateStates()` публикует нормализованное фактическое состояние.

### Приложение F. Каркас `WebService`

Каркас использует `SettingsAsync`, как исходный проект. Поля WiFi/MQTT универсальны; `Device` menu заменяется полями нового предметного контроллера.

`network/web_service.h`:

```cpp
#pragma once

#include <Arduino.h>
#include <SettingsAsync.h>

#include "mqtt_config.h"
#include "wifi_config.h"

class DeviceController;
class EepromStore;
class MqttService;
class SettingsRepository;
class WifiService;

class WebService {
public:
  WebService(
    EepromStore& eeprom,
    SettingsRepository& settings,
    DeviceController& device,
    MqttService& mqtt,
    SettingsAsync& webSettings,
    WifiService& wifi
  )
    : eeprom_(eeprom),
      settings_(settings),
      device_(device),
      mqtt_(mqtt),
      webSettings_(webSettings),
      wifi_(wifi) {}

  void init();
  void tick();

private:
  EepromStore& eeprom_;
  SettingsRepository& settings_;
  DeviceController& device_;
  MqttService& mqtt_;
  SettingsAsync& webSettings_;
  WifiService& wifi_;

  char inputWifiSsid_[WIFI_SSID_LEN] = {};
  char inputWifiPass_[WIFI_PASS_LEN] = {};
  char inputMqttHost_[MQTT_HOST_LEN] = {};
  char inputMqttPort_[MQTT_PORT_LEN] = {};
  char inputMqttUser_[MQTT_USER_LEN] = {};
  char inputMqttPass_[MQTT_PASS_LEN] = {};

  void loadInputBuffers();
  void settingsBuilder(sets::Builder& builder);
  void settingsUpdate(sets::Updater& updater);
};
```

`network/web_service.cpp`:

```cpp
#include "web_service.h"

#include "../config.h"
#include "../core/device_controller.h"
#include "../storage/eeprom_store.h"
#include "../storage/settings_repository.h"
#include "mqtt_service.h"
#include "wifi_service.h"

void WebService::init() {
  loadInputBuffers();

  webSettings_.begin(true, wifi_.getDeviceId().c_str());
  webSettings_.onBuild([this](sets::Builder& builder) { settingsBuilder(builder); });
  webSettings_.onUpdate([this](sets::Updater& updater) { settingsUpdate(updater); });
  webSettings_.setTitle(DEVICE_NAME);
  webSettings_.setVersion(FIRMWARE_VERSION);
}

void WebService::tick() {
  webSettings_.tick();
}

void WebService::loadInputBuffers() {
  const WifiConfig wifiConfig = eeprom_.readWifiConfig();
  strlcpy(inputWifiSsid_, wifiConfig.ssid, sizeof(inputWifiSsid_));
  strlcpy(inputWifiPass_, wifiConfig.password, sizeof(inputWifiPass_));

  const MqttConfig mqttConfig = eeprom_.readMqttConfig();
  strlcpy(inputMqttHost_, mqttConfig.host, sizeof(inputMqttHost_));
  strlcpy(inputMqttPort_, mqttConfig.port, sizeof(inputMqttPort_));
  strlcpy(inputMqttUser_, mqttConfig.user, sizeof(inputMqttUser_));
  strlcpy(inputMqttPass_, mqttConfig.password, sizeof(inputMqttPass_));
}

void WebService::settingsBuilder(sets::Builder& builder) {
  {
    sets::Menu menu(builder, "Device");

    bool powerOn = device_.isPowerOn();
    if (builder.Switch("Power", &powerOn)) {
      device_.setPower(powerOn);
    }

    // Добавить предметные поля. Все команды направлять через DeviceController.
  }

  {
    sets::Menu menu(builder, "WiFi");
    builder.Input("SSID", inputWifiSsid_);
    builder.Pass("Password", inputWifiPass_);

    if (builder.Button("Save and restart")) {
      if (eeprom_.writeWifiConfig(inputWifiSsid_, inputWifiPass_)) {
        settings_.saveNow();
        ESP.restart();
      }
    }
  }

  {
    sets::Menu menu(builder, "MQTT");
    builder.Input("Host", inputMqttHost_);
    builder.Number("Port", inputMqttPort_);
    builder.Input("User", inputMqttUser_);
    builder.Pass("Password", inputMqttPass_);

    if (builder.Button("Save and restart")) {
      if (eeprom_.writeMqttConfig(
            inputMqttHost_,
            inputMqttPort_,
            inputMqttUser_,
            inputMqttPass_
          )) {
        settings_.saveNow();
        ESP.restart();
      }
    }
  }

  {
    sets::Menu menu(builder, "Information");
    builder.Label("Device ID", wifi_.getDeviceId());
    builder.Label("IP", WiFi.localIP().toString());
    builder.Label("RSSI", String(WiFi.RSSI()) + " dBm");
    builder.Label("Firmware", FIRMWARE_VERSION);
  }
}

void WebService::settingsUpdate(sets::Updater& updater) {
  (void)updater;
  // Заполнить только если библиотека требует push-обновления отдельных полей.
}
```

Сигнатуры отдельных widgets зависят от закреплённой версии `SettingsAsync`. Перед переносом сверить их с локальным source tree PlatformIO dependency. Версию библиотеки закрепить в `platformio.ini`.

Если проект не должен перезагружаться после изменения MQTT config, заменить restart на `mqtt_.reloadConfig()`. Для WiFi предпочтителен `WifiService::reloadConfig()`, а не прямые вызовы ESP WiFi API из WebService.

### Приложение G. Что переносится без изменений, что адаптируется

Переносить почти буквально:

- `.clang-format`;
- `wifi_config.h` и WiFi state machine;
- OTA lifecycle;
- структура EEPROM blocks и compile-time checks;
- delayed-save механизм `SettingsRepository`;
- composition root и `init()`/`tick()` pattern;
- разделение PlatformIO config/local/example.

Адаптировать:

- `PersistedDeviceSettings` и device EEPROM block;
- `DeviceController` API;
- MQTT entities, topics и command callbacks;
- Web `Device` menu;
- hardware drivers и GPIO;
- safe-state действия при OTA start;
- ESP8266/ESP32 platform includes.

Не переносить старые предметные классы только ради сохранения формы каталогов. Новый модуль создаётся, когда у нового устройства есть соответствующая ответственность.

## Практический баланс

Не нужно добавлять интерфейс на каждый класс, контейнер dependency injection, event bus или универсальную framework-обвязку без реальной необходимости. Для ESP важнее:

- явное владение;
- ограниченное использование памяти;
- предсказуемый порядок запуска;
- короткий `loop()`;
- отсутствие блокирующих reconnect;
- единый источник истины;
- безопасное и версионированное хранение;
- одинаковое поведение независимо от источника команды.

Если новый проект мал, классы могут быть компактными. Границы ответственности и потоки данных при этом должны сохраниться.
