'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const root = path.resolve(__dirname, '..', '..');
const parityDir = __dirname;
const fastledRoot = path.join(root, '.pio', 'libdeps', 'lamp1_ota', 'FastLED');
const fastledSrc = path.join(fastledRoot, 'src');
const goldenPath = path.join(parityDir, 'golden', 'fastled-lamp1_ota.json');
const sourcePaths = [
  'fastled_config.h',
  'platforms/math8.h',
  'platforms/trig8.h',
  'platforms/shared/scale8.h',
  'platforms/shared/trig8.h',
  'platforms/shared/math8.h',
  'lib8tion.h',
  'fl/math/beat.h',
  'fl/math/xymap.h',
  'fl/math/xymap.cpp.hpp',
  'fl/stl/shared_ptr.cpp.hpp',
  'fl/gfx/colorutils.cpp.hpp',
  'fl/gfx/blur.cpp.hpp',
  'hsv2rgb.cpp.hpp',
  'noise.cpp.hpp',
  'noise.h',
  'crgb.cpp.hpp',
];

function run(command, args, options = {}) {
  return execFileSync(command, args, { cwd: root, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'], ...options }).trim();
}

function hash(file) {
  return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
}

function sourceHashes() {
  const hashes = {};
  for (const relativePath of sourcePaths) {
    const source = path.join(fastledSrc, relativePath);
    if (!fs.existsSync(source)) throw new Error(`FastLED source missing: ${source}`);
    hashes[relativePath] = hash(source);
  }
  return hashes;
}

function referenceCompileMacros() {
  // This executable runs on host, so it intentionally does not select FastLED's
  // ESP8266 platform headers. reference_vectors.cpp proves the shared C branches
  // that both this host build and the non-AVR ESP8266 math path use.
  return {
    ARDUINO: '10805',
    SKETCH_HAS_LARGE_MEMORY: '0',
  };
}

function referenceConfiguration() {
  const config = fs.readFileSync(path.join(fastledSrc, 'fastled_config.h'), 'utf8');
  for (const macro of ['FASTLED_SCALE8_FIXED', 'FASTLED_BLEND_FIXED', 'FASTLED_NOISE_FIXED', 'FASTLED_NOISE_ALLOW_AVERAGE_TO_OVERFLOW']) {
    if (!new RegExp(`#define\\s+${macro}\\s+1\\b`).test(config)) throw new Error(`FastLED config incompatible: ${macro} must be 1`);
  }
  return {
    FASTLED_SCALE8_FIXED: '1',
    FASTLED_BLEND_FIXED: '1',
    FASTLED_NOISE_FIXED: '1',
    FASTLED_NOISE_ALLOW_AVERAGE_TO_OVERFLOW: '0',
  };
}

function compile(output, source, includes, defines = []) {
  const args = ['-std=c++17', '-O2', '-Wall', '-Wextra', '-ffunction-sections', ...defines.flatMap((define) => [`-D${define}`])];
  for (const include of includes) args.push('-I', include);
  const deadCodeElimination = process.platform === 'darwin' ? '-Wl,-dead_strip' : '-Wl,--gc-sections';
  args.push(source, deadCodeElimination, '-o', output);
  run('c++', args);
}

function produceShim(tempDir) {
  // Build and execute independently: reference and shim symbols never link
  // into the same executable.
  const executable = path.join(tempDir, 'shim-vectors');
  compile(executable, path.join(parityDir, 'shim_vectors.cpp'), [
    path.join(root, 'sim', 'host', 'shims'),
    path.join(root, 'sim', 'common'),
    path.join(root, 'sim', 'host', 'src'),
    parityDir,
  ]);
  return JSON.parse(run(executable, []));
}

function produceReference(tempDir) {
  const executable = path.join(tempDir, 'reference-vectors');
  compile(executable, path.join(parityDir, 'reference_vectors.cpp'), [
    path.join(parityDir, 'reference_arduino'),
    fastledSrc,
    parityDir,
  ], Object.entries(referenceCompileMacros()).map(([name, value]) => `${name}=${value}`));
  return JSON.parse(run(executable, []));
}

function comparable(vectors) {
  return JSON.stringify(vectors);
}

function canonicalVectors(vectors) {
  return Object.fromEntries(Object.entries(vectors).map(([name, value]) => {
    const bytes = Buffer.from(value, 'base64');
    return [name, { bytes: bytes.length, sha256: crypto.createHash('sha256').update(bytes).digest('hex') }];
  }));
}

function assertGolden(shim, golden) {
  assert.deepStrictEqual(canonicalVectors(shim.vectors), golden.vectors, 'shim vectors differ from committed FastLED golden');
  assert.deepStrictEqual(shim.sentinels, golden.sentinels, 'shim sentinels differ from committed FastLED golden');
}

function main() {
  const mode = process.argv[2] || 'check';
  if (!['check', '--reference', '--print-golden', '--diagnose'].includes(mode)) throw new Error(`Unknown mode: ${mode}`);
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'gyverlamp-fastled-parity-'));
  try {
    const shim = produceShim(tempDir);
    if (mode === 'check') {
      assertGolden(shim, JSON.parse(fs.readFileSync(goldenPath, 'utf8')));
      process.stdout.write('FastLED shim parity golden: passed\n');
      return;
    }

    const golden = JSON.parse(fs.readFileSync(goldenPath, 'utf8'));
    const provenance = {
      commit: run('git', ['-C', fastledRoot, 'rev-parse', 'HEAD']),
      referenceCompileMacros: referenceCompileMacros(),
      fastledConfiguration: referenceConfiguration(),
      sharedBranches: {
        scale8: 'platforms/shared/scale8.h (SCALE8_C=1)',
        trig8: 'platforms/shared/trig8.h (!__AVR__ && !USE_SIN_32)',
        sketchHasLargeMemory: '0',
      },
      sources: sourceHashes(),
    };
    if (mode === '--reference' && comparable(provenance) !== comparable(golden.provenance)) {
      throw new Error('installed FastLED provenance differs from committed golden');
    }
    const reference = produceReference(tempDir);
    if (mode === '--diagnose') {
      const differences = {};
      for (const key of Object.keys(shim.vectors)) {
        const actual = shim.vectors[key];
        const expected = reference.vectors[key];
        if (actual === expected) continue;
        const actualBytes = Buffer.from(actual, 'base64');
        const expectedBytes = Buffer.from(expected, 'base64');
        let offset = 0;
        while (offset < actualBytes.length && actualBytes[offset] === expectedBytes[offset]) ++offset;
        differences[key] = { offset, actual: [...actualBytes.subarray(offset, offset + 12)], expected: [...expectedBytes.subarray(offset, offset + 12)] };
      }
      for (const key of Object.keys(shim.sentinels)) {
        if (shim.sentinels[key] !== reference.sentinels[key]) differences[`sentinels.${key}`] = { actual: shim.sentinels[key], expected: reference.sentinels[key] };
      }
      process.stdout.write(`${JSON.stringify(differences, null, 2)}\n`);
      return;
    }
    assert.deepStrictEqual(shim, reference, 'independent shim and FastLED reference vectors differ');
    if (mode === '--reference') assertGolden(reference, golden);
    if (mode === '--print-golden') {
      process.stdout.write(`${JSON.stringify({ schema: 2, provenance, vectors: canonicalVectors(shim.vectors), sentinels: shim.sentinels }, null, 2)}\n`);
    } else {
      process.stdout.write('FastLED shim parity reference: passed\n');
    }
  } finally {
    fs.rmSync(tempDir, { recursive: true, force: true });
  }
}

try {
  main();
} catch (error) {
  process.stderr.write(`FastLED shim parity: ${error.message}\n`);
  process.exitCode = 1;
}
