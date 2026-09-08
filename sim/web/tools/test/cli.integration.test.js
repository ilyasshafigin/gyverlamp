'use strict';

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');
const test = require('node:test');
const { PNG } = require('pngjs');
const { loadWasm } = require('../lib/wasm-loader');
const { MAX_MEMORY_BYTES, estimateMemoryBytes } = require('../lib/export-job');

const webRoot = path.resolve(__dirname, '..', '..');
const cli = path.join(webRoot, 'tools', 'render-effect.js');
const wasm = path.join(webRoot, 'public', 'wasm', 'gyverlamp_sim_wasm.wasm');
const glue = path.join(webRoot, 'public', 'wasm', 'gyverlamp_sim_wasm.js');
const identity = path.join(webRoot, 'public', 'wasm', 'gyverlamp_sim_wasm.identity.json');

function digest(file) {
  return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
}

function render(root, name, request, env = {}) {
  const requestPath = path.join(root, `${name}.json`);
  fs.writeFileSync(requestPath, JSON.stringify(request));
  return JSON.parse(execFileSync(process.execPath, [cli, 'effect', '--request', requestPath, '--out', path.join(root, name)], {
    cwd: os.tmpdir(),
    env: { ...process.env, ...env },
  }));
}

function manifest(root, name, result) {
  return JSON.parse(fs.readFileSync(path.join(root, name, result.manifest)));
}

test('required WASM and identity exist', () => {
  assert.ok(fs.existsSync(wasm), 'WASM missing: run npm run build:wasm');
  assert.ok(fs.existsSync(identity), 'WASM identity missing: run npm run build:wasm');
});

test('fresh CLI processes produce equal raw RGB, PNG bytes and manifest', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'gyverlamp-export-'));
  try {
    const request = {
      schemaVersion: 1,
      effectId: 0,
      seed: 123,
      clockStartUtc: '2024-01-02T03:04:05.006Z',
      atMs: [0, 99],
      views: ['logical', 'sharp-v1', 'diffuser-v1'],
      previewScale: 2,
    };
    const first = render(root, 'one', request);
    const second = render(root, 'two', request);
    const firstManifest = path.join(root, 'one', first.manifest);
    const secondManifest = path.join(root, 'two', second.manifest);
    assert.equal(digest(firstManifest), digest(secondManifest));
    const manifest = JSON.parse(fs.readFileSync(firstManifest));
    assert.equal(manifest.frames.length, 2);
    for (const frame of manifest.frames) {
      for (const image of frame.images) {
        const firstPng = path.join(root, 'one', first.output, image.path);
        const secondPng = path.join(root, 'two', second.output, image.path);
        assert.equal(digest(firstPng), digest(secondPng));
        assert.equal(digest(firstPng), image.pngSha256);
      }
    }
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('capture is timestamp-local, initial values snap, and clock ignores TZ', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'gyverlamp-export-'));
  try {
    const base = {
      schemaVersion: 1,
      effectId: 15,
      seed: 99,
      clockStartUtc: '2024-01-02T03:04:05.006Z',
      views: ['logical'],
      previewScale: 1,
    };
    const single = render(root, 'single', { ...base, atMs: [99] });
    const multi = render(root, 'multi', { ...base, atMs: [0, 99] });
    assert.equal(manifest(root, 'single', single).frames[0].rgbSha256, manifest(root, 'multi', multi).frames[1].rgbSha256);
    const dark = render(root, 'dark', { ...base, effectId: 0, brightness: 0, atMs: [0] });
    assert.equal(manifest(root, 'dark', dark).frames[0].appliedOutputBrightness, 0);
    const midpoint = render(root, 'midpoint', { ...base, effectId: 0, brightness: 128, atMs: [0] });
    const full = render(root, 'full', { ...base, effectId: 0, brightness: 255, atMs: [0] });
    assert.equal(manifest(root, 'midpoint', midpoint).frames[0].appliedOutputBrightness, 127);
    assert.equal(manifest(root, 'full', full).frames[0].appliedOutputBrightness, 254);
    const utc = render(root, 'utc', { ...base, atMs: [99] }, { TZ: 'UTC' });
    const tokyo = render(root, 'tokyo', { ...base, atMs: [99] }, { TZ: 'Asia/Tokyo' });
    assert.equal(manifest(root, 'utc', utc).frames[0].rgbSha256, manifest(root, 'tokyo', tokyo).frames[0].rgbSha256);
    const stochastic = { ...base, effectId: 3, atMs: [1000] };
    const seedOne = render(root, 'seed-one', { ...stochastic, seed: 1 });
    const seedTwo = render(root, 'seed-two', { ...stochastic, seed: 2 });
    assert.notEqual(manifest(root, 'seed-one', seedOne).frames[0].rgbSha256, manifest(root, 'seed-two', seedTwo).frames[0].rgbSha256);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('catalog smoke writes valid nonzero-frame logical PNG for every effect', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'gyverlamp-catalog-'));
  try {
    const catalog = JSON.parse(execFileSync(process.execPath, [cli, 'catalog', '--json'])).catalog;
    for (const effect of catalog.effects) {
      const result = render(root, `effect-${effect.id}`, {
        schemaVersion: 1,
        effectId: effect.id,
        seed: 1,
        clockStartUtc: '2024-01-02T03:04:05.006Z',
        atMs: [20],
        views: ['logical'],
        previewScale: 1,
      });
      const data = manifest(root, `effect-${effect.id}`, result);
      const image = data.frames[0].images[0];
      const decoded = PNG.sync.read(fs.readFileSync(path.join(root, `effect-${effect.id}`, result.output, image.path)));
      assert.equal(decoded.width, data.geometry.width);
      assert.equal(decoded.height, data.geometry.height);
    }
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('CLI rejects stale catalog IDs without publishing an output directory', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'gyverlamp-invalid-'));
  try {
    const requestPath = path.join(root, 'request.json');
    const out = path.join(root, 'out');
    fs.writeFileSync(requestPath, JSON.stringify({
      schemaVersion: 1,
      effectId: 255,
      seed: 1,
      clockStartUtc: '2024-01-02T03:04:05.006Z',
      atMs: [0],
      views: ['logical'],
    }));
    assert.throws(() => execFileSync(process.execPath, [cli, 'effect', '--request', requestPath, '--out', out], { cwd: os.tmpdir() }));
    assert.equal(fs.existsSync(out), false);
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('job ABI snaps initial effect settings and rejects duplicate init/backwards advance', async () => {
  const { module } = await loadWasm();
  const number = (name, args = []) => module.cwrap(name, 'number', args);
  const init = number('sim_job_init', ['number', 'number', 'number', 'number', 'number', 'number', 'number']);
  const advance = number('sim_job_advance_to', ['number']);
  assert.equal(init(4, 2, 111, 112, 113, 7, Date.parse('2024-01-02T03:04:05.006Z')), 1);
  assert.equal(number('sim_job_effect_id')(), 4);
  assert.equal(number('sim_job_palette_id')(), 2);
  assert.equal(number('sim_job_brightness')(), 111);
  assert.equal(number('sim_job_speed')(), 112);
  assert.equal(number('sim_job_scale')(), 113);
  assert.equal(init(4, 2, 111, 112, 113, 7, Date.parse('2024-01-02T03:04:05.006Z')), 0);
  assert.equal(advance(20), 1);
  assert.equal(advance(19), 0);
});

test('legacy browser Clock completes transition and reads host local clock', () => {
  const loader = path.join(webRoot, 'tools', 'lib', 'wasm-loader.js');
  const program = `const {loadWasm}=require(${JSON.stringify(loader)});(async()=>{const {module}=await loadWasm();const n=(x,a=[])=>module.cwrap(x,'number',a);n('sim_init')();n('sim_set_effect',['number'])(15);n('sim_tick',['number'])(0);n('sim_tick',['number'])(400);n('sim_tick',['number'])(800);console.log(JSON.stringify({active:n('sim_active_effect_id')(),deterministic:n('sim_clock_is_deterministic')(),hours:n('sim_time_hours')(),minutes:n('sim_time_minutes')(),expectedHours:new Date().getHours(),expectedMinutes:new Date().getMinutes()}));})().catch(e=>{console.error(e);process.exit(1)});`;
  const run = (TZ) => JSON.parse(execFileSync(process.execPath, ['-e', program], { env: { ...process.env, TZ } }));
  const utc = run('UTC');
  const tokyo = run('Asia/Tokyo');
  for (const result of [utc, tokyo]) {
    assert.equal(result.active, 15);
    assert.equal(result.deterministic, 0);
    assert.equal(result.hours, result.expectedHours);
    assert.equal(result.minutes, result.expectedMinutes);
  }
  assert.notEqual(utc.hours, tokyo.hours);
});

test('binary hash mismatch blocks CLI before output publication', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'gyverlamp-stale-'));
  const original = fs.readFileSync(wasm);
  try {
    fs.writeFileSync(wasm, Buffer.concat([original, Buffer.from([0]) ]));
    const requestPath = path.join(root, 'request.json');
    const out = path.join(root, 'out');
    fs.writeFileSync(requestPath, JSON.stringify({ schemaVersion: 1, effectId: 0, seed: 1, clockStartUtc: '2024-01-02T03:04:05.006Z', atMs: [0], views: ['logical'] }));
    assert.throws(() => execFileSync(process.execPath, [cli, 'effect', '--request', requestPath, '--out', out], { cwd: os.tmpdir() }));
    assert.equal(fs.existsSync(out), false);
  } finally {
    fs.writeFileSync(wasm, original);
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('glue hash mismatch blocks CLI before output publication', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'gyverlamp-glue-'));
  const original = fs.readFileSync(glue);
  try {
    fs.writeFileSync(glue, Buffer.concat([original, Buffer.from('\n// stale\n')]));
    const requestPath = path.join(root, 'request.json');
    const out = path.join(root, 'out');
    fs.writeFileSync(requestPath, JSON.stringify({ schemaVersion: 1, effectId: 0, seed: 1, clockStartUtc: '2024-01-02T03:04:05.006Z', atMs: [0], views: ['logical'] }));
    assert.throws(() => execFileSync(process.execPath, [cli, 'effect', '--request', requestPath, '--out', out], { cwd: os.tmpdir() }));
    assert.equal(fs.existsSync(out), false);
  } finally {
    fs.writeFileSync(glue, original);
    fs.rmSync(root, { recursive: true, force: true });
  }
});

test('largest admitted diffuser job publishes under conservative cap', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'gyverlamp-budget-'));
  try {
    const atMs = [...Array(120)].map((_, index) => index * 20);
    const estimatedBytes = estimateMemoryBytes(16, 16, atMs.length, 8, ['diffuser-v1']);
    assert.ok(estimatedBytes <= MAX_MEMORY_BYTES);
    const result = render(root, 'boundary', {
      schemaVersion: 1,
      effectId: 0,
      seed: 1,
      clockStartUtc: '2024-01-02T03:04:05.006Z',
      atMs,
      views: ['diffuser-v1'],
      previewScale: 8,
    });
    const data = manifest(root, 'boundary', result);
    assert.equal(data.outputBudget.estimatedBytes, estimatedBytes);
    assert.equal(data.contactSheet.sourceLevel, 'diffuser-v1');
  } finally {
    fs.rmSync(root, { recursive: true, force: true });
  }
});
