'use strict';

const assert = require('assert');
const test = require('node:test');

const { canonicalJson, validateRequest, verifyWorkLimits } = require('../lib/request');
const { MAX_MEMORY_BYTES, estimateMemoryBytes } = require('../lib/export-job');
const { diffuse, rgbaFromRgb, scaleByte, upscaleNearest } = require('../lib/png-renderer');

const baseRequest = {
  schemaVersion: 1,
  effectId: 0,
  seed: 7,
  clockStartUtc: '2024-01-02T03:04:05.006Z',
  atMs: [0, 33],
  views: ['logical', 'sharp-v1', 'diffuser-v1'],
};

test('canonical JSON ignores source key order', () => {
  assert.equal(canonicalJson({ b: [2, { z: 1, a: 2 }], a: true }), canonicalJson({ a: true, b: [2, { a: 2, z: 1 }] }));
});

test('request rejects unknown fields, reordered timestamps, and work excess', () => {
  assert.throws(() => validateRequest({ ...baseRequest, extra: true }), /unknown field/);
  assert.throws(() => validateRequest({ ...baseRequest, schemaVersion: 2 }), /schemaVersion/);
  assert.throws(() => validateRequest({ ...baseRequest, effectId: 256 }), /effectId/);
  assert.throws(() => validateRequest({ ...baseRequest, brightness: -1 }), /brightness/);
  assert.throws(() => validateRequest({ ...baseRequest, seed: 0 }), /seed/);
  assert.throws(() => validateRequest({ ...baseRequest, seed: 0x100000000 }), /seed/);
  assert.throws(() => validateRequest({ ...baseRequest, atMs: [33, 0] }), /sorted and unique/);
  assert.throws(() => validateRequest({ ...baseRequest, atMs: [600001] }), /maxSimulationMs/);
  assert.throws(() => validateRequest({ ...baseRequest, clockStartUtc: '2024-01-02T03:04:05Z' }), /UTC format/);
  assert.equal(verifyWorkLimits(validateRequest({ ...baseRequest, atMs: [600000] }), 20).projectedInternalSteps, 30001);
  assert.throws(() => verifyWorkLimits(validateRequest({ ...baseRequest, atMs: [600000] }), 19), /maxInternalSteps/);
});

test('brightness scale and nearest mapping are deterministic', () => {
  assert.equal(scaleByte(255, 0), 0);
  assert.equal(scaleByte(255, 128), 128);
  assert.equal(scaleByte(255, 255), 255);
  const image = upscaleNearest(rgbaFromRgb(Uint8Array.from([1, 2, 3, 4, 5, 6])), 2, 1, 2);
  assert.deepEqual([...image.data.slice(0, 16)], [1, 2, 3, 255, 1, 2, 3, 255, 4, 5, 6, 255, 4, 5, 6, 255]);
});

test('output budget accepts exact largest boundary and rejects next scale', () => {
  const accepted = [...Array(64)].map((_, index) => index + 1).filter((scale) =>
    estimateMemoryBytes(16, 16, 120, scale, ['diffuser-v1']) <= MAX_MEMORY_BYTES,
  ).pop();
  assert.equal(accepted, 8);
  assert.ok(estimateMemoryBytes(16, 16, 120, accepted, ['diffuser-v1']) <= MAX_MEMORY_BYTES);
  assert.ok(estimateMemoryBytes(16, 16, 120, accepted + 1, ['diffuser-v1']) > MAX_MEMORY_BYTES);
});

test('diffuser vectors preserve linear input, edge rules and physical footprint', () => {
  const black = diffuse(new Uint8Array(3 * 2 * 2), 2, 2, 2, 255);
  assert.ok(black.data.every((value, index) => index % 4 === 3 || value === 0));
  const white = diffuse(new Uint8Array(3 * 2 * 2).fill(255), 2, 2, 2, 255);
  assert.ok(white.data.every((value, index) => index % 4 === 3 || value === 255));
  const impulse = new Uint8Array(3 * 3 * 3);
  impulse[0] = 255;
  const wrapped = diffuse(impulse, 3, 3, 2, 255);
  const oppositeEdge = (1 * wrapped.width + wrapped.width - 1) * 4;
  assert.ok(wrapped.data[oppositeEdge] > 0, 'X blur wraps to opposite edge');
  const topImpulse = new Uint8Array(3 * 3 * 3);
  topImpulse[3] = 255;
  const clamped = diffuse(topImpulse, 3, 3, 1, 255);
  assert.ok(clamped.data[4] > clamped.data[(2 * 3 + 1) * 4], 'Y blur clamps at edge');
  const checker = Uint8Array.from([255, 0, 0, 0, 0, 0, 0, 0, 0, 255, 0, 0]);
  assert.notEqual(diffuse(checker, 2, 2, 1, 128).data[0], diffuse(checker, 2, 2, 1, 255).data[0]);
  const one = diffuse(impulse, 3, 3, 1, 255);
  const three = diffuse(impulse, 3, 3, 3, 255);
  for (let y = 0; y < 3; y += 1) {
    for (let x = 0; x < 3; x += 1) assert.equal(one.data[(y * 3 + x) * 4], three.data[((y * 3) * three.width + x * 3) * 4]);
  }
});
