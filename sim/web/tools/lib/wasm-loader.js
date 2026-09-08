'use strict';

const fs = require('fs');
const path = require('path');

const { verifyIdentity } = require('./simulator-identity');

const WASM_DIR = path.resolve(__dirname, '../../public/wasm');
const WASM_JS = path.join(WASM_DIR, 'gyverlamp_sim_wasm.js');
const WASM_BINARY = path.join(WASM_DIR, 'gyverlamp_sim_wasm.wasm');

function missingWasmError() {
  return new Error(`WASM simulator is missing. Run npm run build:wasm from sim/web; expected ${WASM_JS} and ${WASM_BINARY}`);
}

async function loadWasm() {
  if (!fs.existsSync(WASM_JS) || !fs.existsSync(WASM_BINARY)) throw missingWasmError();
  const identity = verifyIdentity();
  const factory = require(WASM_JS);
  if (typeof factory !== 'function') throw new Error(`invalid Emscripten module factory: ${WASM_JS}`);
  const module = await factory({
    locateFile(file) {
      return path.join(WASM_DIR, file);
    },
  });
  return {
    module,
    wasmPath: WASM_BINARY,
    wasmBytes: fs.readFileSync(WASM_BINARY),
    identity,
  };
}

function catalogFromModule(module) {
  const number = (name, args = []) => module.cwrap(name, 'number', args);
  const string = (name, args = []) => module.cwrap(name, 'string', args);
  const effectCount = number('sim_effect_count')();
  const effectIdAt = number('sim_effect_id_at', ['number']);
  const effectNameAt = string('sim_effect_name_at', ['number']);
  const defaultBrightness = number('sim_effect_default_brightness', ['number']);
  const defaultSpeed = number('sim_effect_default_speed', ['number']);
  const defaultScale = number('sim_effect_default_scale', ['number']);
  const paletteCount = number('sim_palette_count')();
  const paletteIdAt = number('sim_palette_id_at', ['number']);
  const paletteNameAt = string('sim_palette_name_at', ['number']);
  const effects = [];
  const palettes = [];
  for (let index = 0; index < effectCount; index += 1) {
    effects.push({
      id: effectIdAt(index),
      name: effectNameAt(index),
      settings: { brightness: defaultBrightness(index), speed: defaultSpeed(index), scale: defaultScale(index) },
    });
  }
  for (let index = 0; index < paletteCount; index += 1) {
    palettes.push({ id: paletteIdAt(index), name: paletteNameAt(index) });
  }
  return { schemaVersion: 1, effects, palettes };
}

module.exports = { WASM_BINARY, WASM_JS, catalogFromModule, loadWasm, missingWasmError };
