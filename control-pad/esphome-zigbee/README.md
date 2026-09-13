# Control Pad: ESPHome, Zigbee2MQTT и Home Assistant

Этот каталог содержит текущий вариант Control Pad: ESPHome с пользовательским Zigbee-компонентом, Zigbee2MQTT и Home Assistant. Материалы служат справочными примерами для повторения панели.

## Содержимое

- [`esphome/`](esphome/) — конфигурации панели для ESPHome:
  - [`control_pad-batt.yaml`](esphome/control_pad-batt.yaml) — вариант с питанием от аккумулятора;
  - [`control_pad-enc.yaml`](esphome/control_pad-enc.yaml) — вариант с энкодером EC11.
- [`zigbee2mqtt/`](zigbee2mqtt/) — внешние конвертеры Zigbee2MQTT: [`control_pad_6.js`](zigbee2mqtt/control_pad_6.js) и [`control_pad_5_enc.js`](zigbee2mqtt/control_pad_5_enc.js).
- [`homeassistant/`](homeassistant/) — примеры автоматизаций для [варианта с аккумулятором](homeassistant/control_pad-batt.yaml) и [варианта с энкодером](homeassistant/control_pad-enc.yaml), конфигурация для лампы [`gyverlamp.yaml`](homeassistant/gyverlamp.yaml), а также [blueprints](homeassistant/blueprints/automation/ilyasshafigin/).

## Варианты конфигурации

### Питание от аккумулятора

Конфигурация [`control_pad-batt.yaml`](esphome/control_pad-batt.yaml) предназначена для автономной панели. В собранном варианте аккумулятор 400 мА·ч работал не более трёх дней, это максимум чтого получилось добиться.

### Энкодер

Конфигурация [`control_pad-enc.yaml`](esphome/control_pad-enc.yaml) добавляет энкодер EC11 (может работать с совместимыми энкодерами). Связанные примеры интеграции находятся в [`control_pad_5_enc.js`](zigbee2mqtt/control_pad_5_enc.js) и [`control_pad-enc.yaml`](homeassistant/control_pad-enc.yaml).

## Полезные ссылки

- [Корпус для 6-кнопочной панели](https://makerworld.com/en/models/2725522-macro-pad-3x2-macro-keyboard-multiple-layers)
- [Корпус для версии с энкодером](https://www.thingiverse.com/thing:4416966)
- [Пользовательский Zigbee-компонент](https://github.com/luar123/zigbee_esphome)
- [Генератор SVG-легенд для кейкапов](https://vostoklabs.github.io/SVG-keycap-generator/)
- [Инструкция по ESPHome clicker](https://albert.nz/esphome-button-xiaomi-zigbee)
