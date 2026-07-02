---
name: create-empty-effect
description: Create empty GyverLamp effect skeletons. Use when user asks to add/create/new empty effect, effect files, Effects::Id, or effect registry entries in this PlatformIO GyverLamp repo.
---

# Create Empty Effect

Use this skill to add a new placeholder effect to this GyverLamp firmware.

## Inputs

Need one name:

- English effect name for C++ class and UI/MQTT display, e.g. `Matrix Rain`, `Spark Pulse`, `EffectSparkPulse`.

If user passed a name in command/request, do not ask.

If name missing, ask one targeted question:

```text
Need effect name in English for class and display.
Example: Spark Pulse
```

## Naming rules

Normalize English name into three forms:

- `ClassName`: PascalCase, prefixed with `Effect` exactly once.
  - `Spark Pulse` -> `EffectSparkPulse`
  - `EffectSparkPulse` -> `EffectSparkPulse`
- `file_stem`: snake_case without `effect_` prefix.
  - `Spark Pulse` -> `spark_pulse`
  - `EffectSparkPulse` -> `spark_pulse`
- `Effects::Id`: PascalCase.
  - `Spark Pulse` -> `SparkPulse`
  - `EffectSparkPulse` -> `SparkPulse`

Derive `NAME` string from `ClassName`: remove the leading `Effect` and split remaining PascalCase words with spaces.
  - `EffectSparkPulse` -> `Spark Pulse`
  - `EffectMatrix` -> `Matrix`

## Files to edit

Create:

- `src/effect/catalog/<file_stem>.h`
- `src/effect/catalog/<file_stem>.cpp`

Update:

- `src/effect/ids.h`
- `src/effect/effects.h`
- `src/effect/catalog.h`

`src/effect/catalog.cpp` usually needs no edit because it uses `EFFECT_REGISTRY` macro from `catalog.h`.

## Header template

Create `src/effect/catalog/<file_stem>.h`:

```cpp
#pragma once

#include "../effect.h"

class <ClassName> : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::<EffectsId>;
  static constexpr const char* NAME = "<English display name>";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    120,  // speed
    40,   // scale
  };
  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
```

Do not add state fields unless user asks. Empty effect should stay minimal.

## Source template

Create `src/effect/catalog/<file_stem>.cpp`:

```cpp
#include "<file_stem>.h"

void <ClassName>::setup(EffectContext& ctx) {
  (void)ctx;
  // TODO: добавить реализацию
}

void <ClassName>::render(EffectContext& ctx) {
  (void)ctx;
  // TODO: добавить реализацию
}
```

Render body must stay empty except `(void)ctx;` and TODO comment above.

## ids.h update

Open `src/effect/ids.h`.

Add new enum item immediately before `COUNT`.

```cpp
  Picasso,
  COUNT,
```

Add:

```cpp
  Picasso,
  <EffectId>,
  COUNT,
```

Do not renumber existing effects unless explicitly requested. EEPROM settings use effect IDs, so stable IDs matter.

## effects.h update

Open `src/effect/effects.h`.

Add include in existing sorted/nearby include block:

```cpp
#include "catalog/<file_stem>.h"
```

Keep one include per line.

## catalog.h update

Open `src/effect/catalog.h`.

Add class to `EFFECT_REGISTRY(X)` so factory, names, settings, parsing, and display order include new effect.

Usually append at end after current last effect:

```cpp
  X(EffectPicasso) \
  X(<ClassName>)
```

If inserting not at end, keep backslashes valid: every registry line except final line must end with `\`.

`DISPLAY_ORDER` and `DISPLAY_COUNT` are generated from this macro; do not edit them separately.

## Verification

Run focused build only:

```bash
pio run -e lamp1_ota
```

Never run plain `pio run`. Never upload/flash unless user explicitly asks.

## Full existing example

Use Matrix as concrete implemented effect reference:

- `src/effect/catalog/matrix.h`
- `src/effect/catalog/matrix.cpp`
- `EffectMatrix`
- `Matrix`
- `NAME = "Matrix"`

Matrix header pattern:

```cpp
#pragma once

#include "../effect.h"

class EffectMatrix : public Effect {
public:
  static constexpr Effects::Id ID = Effects::Id::Matrix;
  static constexpr const char* NAME = "Matrix";
  static constexpr EffectSettingsSpec SETTINGS = {
    180,  // brightness
    16,   // speed
    80,   // scale
  };
  void setup(EffectContext& ctx) override;
  void render(EffectContext& ctx) override;
};
```

For empty effect, do **not** copy Matrix render implementation. Leave `render()` empty with TODO.

## Completion message

After creating effect, report concise:

- created files
- added `Effects::Id`
- added registry/include
- build result
