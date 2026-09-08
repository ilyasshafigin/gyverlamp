---
name: debug-effect-sim
description: Debug or visually inspect a GyverLamp effect with deterministic simulator renders. Use when asked to render an effect, inspect its visual output, compare effect frames, tune effect parameters, or investigate an effect animation.
---

# Debug Effect with Simulator

Use the project-local deterministic exporter before judging an effect visually.
It runs real effect code through the WASM simulator, not browser animation
timing.

## Paths

- Local, disposable requests: `.artifacts/requests/<name>.json`.
- Local render outputs: `.artifacts/sim/` or another `.artifacts/` subdirectory.
- Shared reproducible fixtures: `sim/fixtures/requests/` (tracked).
- Full exporter contract and request schema: `sim/README.md`.

`.artifacts/` is ignored. Do not put reusable fixtures there.

## Workflow

1. Build simulator when WASM may be missing or stale:

   ```bash
   just sim build
   ```

2. Look up effect IDs and default parameters instead of guessing:

   ```bash
   just sim catalog
   ```

3. Create a request in `.artifacts/requests/`. Use a stable non-zero `seed` and
   UTC `clockStartUtc`. Capture several timestamps for animated effects.

4. Render it:

   ```bash
   just sim render \
     --request .artifacts/requests/<name>.json \
     --out .artifacts/sim
   ```

5. Read the output JSON, then inspect the generated `contact-sheet.png`.
   Inspect individual `logical` frames when diagnosing matrix/effect code.
   Read `manifest.json` to reproduce the exact job and inspect resolved values.

6. Run exporter checks after changing simulator/exporter code:

   ```bash
   just sim test
   ```

## Request template

Use numeric persisted IDs. Keep `expectedEffectName` so a changed catalog fails
instead of rendering another effect silently.

```json
{
  "schemaVersion": 1,
  "effectId": 4,
  "expectedEffectName": "Rainbow",
  "paletteId": 0,
  "brightness": 255,
  "speed": 200,
  "scale": 120,
  "seed": 12345,
  "clockStartUtc": "2024-01-02T03:04:05.006Z",
  "atMs": [0, 500, 1000],
  "views": ["logical", "sharp-v1", "diffuser-v1"],
  "previewScale": 8
}
```

Required fields are `schemaVersion`, `effectId`, `seed`, `clockStartUtc`,
`atMs`, and `views`. The exporter rejects unknown fields, invalid IDs/ranges,
out-of-order timestamps, stale/missing WASM identity, and work beyond its
configured limits.

## Reading artifacts

- `logical`: native matrix RGB before output brightness. Use for effect logic,
  XY mapping, palette, and frame-to-frame state.
- `sharp-v1`: logical output after applied brightness, nearest-neighbor scaled.
- `diffuser-v1`: approximate LED/diffuser preview. Use for composition and
  perceived blending, not exact hardware color measurement.
- `contact-sheet.png`: fastest visual overview of captured timestamps.
- `manifest.json`: canonical resolved request, engine identity, geometry/row
  order, RGB hashes, paths, dimensions, and fidelity warnings.

## Debugging rules

- Stateful effects must advance from `t=0`; do not infer a frame at `t=N` from
  a one-off tick.
- Use the same request twice to test reproducibility. Compare manifest and PNG
  hashes before diagnosing a nondeterminism bug.
- Change one input per run: time, seed, palette, brightness, speed, or scale.
- For random effects, record the seed. Some production effects deliberately
  reseed themselves, so document that result rather than adding simulator-only
  behavior.
- For audio effects, use deterministic audio fixtures/configuration from
  `sim/README.md`; browser microphone state is not a reproducible input.

## Fidelity limits

The simulator uses an approximate FastLED shim and does not model real LED
calibration, current limiting, camera exposure, ambient light, or panel-to-panel
variation. It is reliable for effect shape, movement, state, and coarse color;
it is not a colorimetry or power-safety tool.

## Firmware verification

Simulator-only changes need `just sim build` and `just sim test`.
If changing production `src/**`, also run one focused firmware build:

```bash
just build lamp1_ota
```

Do not use plain `pio run` and do not upload unless explicitly requested.
