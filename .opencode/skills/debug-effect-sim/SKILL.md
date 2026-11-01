---
name: debug-effect-sim
description: Debug or visually inspect a GyverLamp effect with deterministic renders. Use when asked to render an effect, inspect its visual output, compare effect frames, tune effect parameters, investigate an animation, or show a synchronized GIF comparing an effect with and without audio.
---

# Debug Effect with Simulator

Use the project-local deterministic exporter for ordinary effect renders. It
runs real effect code through the WASM simulator, not browser animation timing.
For audio influence comparisons, use the native workflow below instead: the
WASM export request does not support an audio timeline.

## Paths

- Local, disposable requests: `.artifacts/requests/<name>.json`.
- Local render outputs: `.artifacts/sim/` or another `.artifacts/` subdirectory.
- Shared reproducible fixtures: `sim/fixtures/requests/` (tracked).
- Generic diagnostic audio fixture: `sim/fixtures/audio/example-three-signal.json`.
- Reusable audio comparison tools: `sim/tools/audio_compare/`.
- Full exporter contract and request schema: `sim/README.md`.

`.artifacts/` is ignored. New experiment-specific files belong there by default,
including source scripts and fixtures. Promote them to shared tracked locations
only with explicit user approval.

## WASM workflow

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

## Audio influence comparisons

Use this path when asked to show how sound changes an effect. It preserves the
two-panel comparison, common clock and visible Level/Bass/Treble inputs.
Read the audio comparison section in `sim/README.md`
for prerequisites and the capture format.

1. Run the self-contained fixed example from the repository root:

   ```bash
   python3 sim/tools/audio_compare/run_example.py \
      --out .artifacts/audio-compare/example
   ```

   It compiles the current production effect against native host shims and
   captures two independent runs from the same seed and settings: audio off
   versus EFFECT. No old `.artifacts/` files, saved C++ snapshots or WASM build
   are required. The input is a deterministic synthetic post-microphone
   AudioFrame timeline, not a microphone recording.

2. Read `capture.json` and `report.json` in the output directory. Inspect exact
   RGB captures and the generated `comparison.gif`. Confirm matching timelines,
   geometry, initial settings and the intended input channels. For a
   reproducibility check, repeat into another output directory and compare the
   two runs' raw RGB and GIF hashes; do not compare path-dependent metadata.
   Keep the Pillow version and font the same when comparing GIF bytes.

3. Show `comparison.gif` inline with `openchamber_render`, using an absolute
   local image path. Keep the common time and input bars visible. Explain which
   side is which, what the inputs mean, and whether the result is actual native
   effect output or an offline visual prototype. Do not describe the first
   differing pixel as a perceptually noticeable response.

The compositor also accepts other paired captures:

```bash
python3 sim/tools/audio_compare/compose.py \
  --manifest <capture.json> --out <output-directory>
```

Existing capture runners target specific effects. Reuse a matching runner when
available; do not claim the fixed example captures arbitrary effects. Its
generic filenames do not make it a universal effect runner; keep actual effect
identity in capture metadata.
For another effect, prepare paired captures using the file placement rules below.
Never present an offline transform as a production-code capture.

### File placement: tools, comparisons and tests

- `sim/tools/audio_compare/` is the shared tool, not an experiment workspace.
  Ordinary use must not add new `capture_*`, `run_*`, `test_*` or per-effect
  check files there. Shared tool tests verify the tool itself, not each effect.
- `.artifacts/audio-compare/<experiment>/` holds temporary comparison work:
  Python/C++ capture scripts, visual prototypes, local input fixtures, generated
  headers, native builds, RGB, GIF and presentation metadata.
- `.artifacts/tests/audio-compare/<experiment>/` holds temporary verification
  work: assertion scripts, per-effect state/regression checks, test harnesses,
  their binaries and reports. Separate these from the visual comparison files.
- File purpose, not extension, determines placement: temporary `.py` and `.cpp`
  sources belong under `.artifacts/` too. A new effect comparison does not imply
  a new permanent runner or permanent test suite.
- Adding shared adapters, tool tests or fixtures requires separate explicit
  approval. Do not relocate or delete existing files as part of applying this
  rule; agree any cleanup independently.

## Reading WASM artifacts

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
- For audio influence, use the native comparison workflow and tracked audio
  fixture above; browser microphone state is not a reproducible input. Do not
  add unsupported audio fields to a WASM export request.

## Fidelity limits

The simulator uses an approximate FastLED shim and does not model real LED
calibration, current limiting, camera exposure, ambient light, or panel-to-panel
variation. It is reliable for effect shape, movement, state, and coarse color;
it is not a colorimetry or power-safety tool.

## Firmware verification

Changes to the WASM simulator/exporter need `just sim build` and `just sim test`.
Changes only to `sim/tools/audio_compare/` need its focused Python tests and a
fresh native example capture; they do not require firmware or WASM builds.

```bash
python3 -m unittest discover -s sim/tools/audio_compare -p 'test_*.py' -v
```

If changing production `src/**`, also run one focused firmware build:

```bash
just lamp build lamp1_ota
```

Do not use plain `pio run` and do not upload unless explicitly requested.
