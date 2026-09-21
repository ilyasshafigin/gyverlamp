# Control Pad

Панель из пяти кнопок и энкодера EC11 для одной GyverLamp. Подходит для новой
сборки «лампа + панель» и для добавления панели к уже работающей лампе.

Также есть пример работы панели [ESPHome/Zigbee](esphome-zigbee/README.md).

## Что потребуется

- лампа на **ESP32 или ESP32-S3** с поддержкой Control Pad;
- ESP32-C3 Super Mini для панели;
- пять MX свитчей или кнопок и EC11/EC12 с кнопкой;
- USB-кабели, провода.

ESP8266 для лампы не поддерживается.
Для ESP32/ESP32-S3 добавьте `-D USE_CONTROL_PAD` только в базовый или
конкретный env этой лампы, от которого наследуется используемый `<lamp-env>`.

## Подключение панели

Все входы панели активны LOW и используют внутреннюю подтяжку: каждая кнопка и
`SW` замыкают свой GPIO на `GND`. Не подавайте на входы 5 В.

| Узел | GPIO ESP32-C3 | Подключение |
| --- | ---: | --- |
| Button 1 | GPIO1 | Второй контакт — GND |
| Button 2 | GPIO2 | Второй контакт — GND |
| Button 3 | GPIO3 | Второй контакт — GND |
| Button 4 | GPIO4 | Второй контакт — GND |
| Button 5 | GPIO5 | Второй контакт — GND |
| EC11 A | GPIO7 | Фаза A |
| EC11 B | GPIO10 | Фаза B |
| EC11 SW | GPIO6 | Второй контакт — GND |
| EC11 common | GND | Общий вывод энкодера |

GPIO8 и GPIO9 оставлены свободными из-за boot strapping ESP32-C3. Статусный LED
по умолчанию отключён; подключать его не требуется.

## Жесты панели

Панель поддерживает: однократное, двухкратное, трехкратное нажатие, удержание.

Конфигурация панели по умолчанию:

| Орган управления | Single click | Double click | Triple click | Hold |
| --- | --- | --- | --- | --- |
| B1 | Вкл./выкл. лампу | Сброс настроек текущего эффекта | — | — |
| B2 | Следующий эффект | — | — | — |
| B3 | Предыдущий эффект | — | — | — |
| B4 | Следующая палитра | Автовыбор палитры | — | — |
| B5 | Вкл./выкл. ротацию эффектов | — | — | — |
| `SW` EC11 | `Brightness → Speed → Scale → Brightness` | Выбирает `Brightness` | — | — |
| Поворот EC11 | Относительно меняет выбранный параметр | — | — | Пока `SW` удерживается, поворот игнорируется |
| B1 + B3 | — | — | — | Удержание вместе 2,5 с запускает pairing |

## Настройка действий кнопок

Каждому жесту B1..B5 соответствует отдельный compile-time macro:
`PANEL_B<1..5>_SINGLE`, `PANEL_B<1..5>_DOUBLE`, `PANEL_B<1..5>_TRIPLE` и
`PANEL_B<1..5>_HOLD`. Добавляйте их в `build_flags` private profile в
`control-pad/platformio.local.ini`. Допустимы только следующие значения:

```ini
PANEL_ACTION_NO_ACTION
PANEL_ACTION_TOGGLE_POWER
PANEL_ACTION_NEXT_EFFECT
PANEL_ACTION_PREVIOUS_EFFECT
PANEL_ACTION_TOGGLE_ROTATION
PANEL_ACTION_NEXT_PALETTE
PANEL_ACTION_SET_PALETTE_AUTO
PANEL_ACTION_RESET_CURRENT_EFFECT_SETTINGS
```

Например, чтобы B2 double click переключал палитру, а B5 hold ничего не
делал:

```ini
[env:panel1]
extends = env:profile
build_flags =
  ${env:profile.build_flags}
  -D PANEL_B2_DOUBLE=PANEL_ACTION_NEXT_PALETTE
  -D PANEL_B5_HOLD=PANEL_ACTION_NO_ACTION
```

`PANEL_ACTION_NO_ACTION` (и неизвестное значение) безопасно игнорируется: в
ESP-NOW не ставится и не отправляется команда. Энкодер не настраивается этими
macros: его кнопка и поворот всегда работают как описано в таблице.

## Профили `diag` и `input`

Оба профиля не требуют credentials, не включают ESP-NOW и не отправляют команд
лампе. Их можно собрать до provisioning:

```bash
just panel build diag
just panel build input
```

`diag` — минимальная проверка проводки: Serial на 115200 показывает начальные
уровни, debounced edges кнопок и switch, а также шаги/направление EC11.
`input` — проверка production-библиотек uButton/uEncButton: Serial показывает
нажатия, длительность, short-кандидаты, hold feedback, mode энкодера, detent и
запрос B1+B3 pairing как `command locked; radio disabled`. Это подтверждает
физические входы и жесты, но не radio/pairing с лампой.

## Сборка и provisioning

1. Соберите лампу с `USE_CONTROL_PAD`:

   ```bash
   just build <lamp-env>
   ```

2. Проверьте проводку панели без radio (не обязательно):

   ```bash
   just panel build diag
   just panel build input
   ```

3. Создайте профиль панели один раз, затем соберите и прошейте его:

    ```bash
    just panel provision <panel-name>
    just panel build <panel-name>
    just panel upload <panel-name> <serial-port>
    ```

   Upload всегда требует явно указанный порт и не запускается без него. Например:

   ```bash
   just panel upload panel1 /dev/cu.usbmodem2101
   ```

Provisioning создаёт private-профиль и pairing-artifact и отказывается их
перезаписывать. Не публикуйте artifact или ключи.
Для action telemetry добавляйте `-D DEBUG` только в `build_flags` нужного
device env, а не в общий `[env]`.

## Первое pairing

1. Прошейте собранную лампу и provisioned панель.
2. Подключите лампу к домашнему Wi-Fi в режиме STA.
3. В Web UI лампы откройте **Control Pad**, вставьте созданный pairing-artifact
   в **Pairing artifact** и нажмите **Open pairing**.
4. Убедитесь, что **Pairing window** показывает `open`. В течение 60 секунд
   удерживайте B1 и B3 одновременно не менее 2,5 секунд.
5. Успех: статус Web UI становится `Bound`, отображаются MAC панели и канал.
   Перезагрузите оба устройства и убедитесь, что `Bound` сохранился.

Если pairing не открылся, проверьте STA лампы и введённый artifact. Если окно
истекло, откройте новое и повторите удержание. Для замены панели или полного
повторного pairing используйте **Clear binding** в Web UI, затем повторите шаги
с существующим artifact; не создавайте профиль с тем же именем заново.

## Проверка железа

1. Откройте Serial панели на 115200 и убедитесь, что все кнопки и `SW` в покое
   показывают HIGH.
2. Проверьте LOW при нажатии каждой кнопки и оба направления EC11 в `diag`.
3. В `input` проверьте границы short/hold, B1+B3 и смену трёх локальных режимов.
4. После pairing проверьте команды, feedback и сохранение binding после reboot.

Сборка не подтверждает качество пайки, питание, радиоканал или pairing на
реальном устройстве: эти пункты требуют физической проверки.
