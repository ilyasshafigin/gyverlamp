'use strict';

const MAX_CAPTURE_COUNT = 120;
const MAX_SIMULATION_MS = 600000;
const MAX_INTERNAL_STEPS = 30001;
const MAX_PREVIEW_SCALE = 64;
const VIEW_NAMES = new Set(['logical', 'sharp-v1', 'diffuser-v1']);

class RequestError extends Error {}

function isObject(value) {
  return value !== null && typeof value === 'object' && !Array.isArray(value);
}

function assertObject(value, label) {
  if (!isObject(value)) throw new RequestError(`${label} must be an object`);
}

function assertKnownFields(value, fields, label) {
  for (const key of Object.keys(value)) {
    if (!fields.has(key)) throw new RequestError(`${label} has unknown field ${JSON.stringify(key)}`);
  }
}

function assertInteger(value, label, min, max) {
  if (!Number.isInteger(value) || value < min || value > max) {
    throw new RequestError(`${label} must be an integer in ${min}..${max}`);
  }
  return value;
}

function parseClockStartUtc(value) {
  if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/.test(value)) {
    throw new RequestError('clockStartUtc must use YYYY-MM-DDTHH:mm:ss.sssZ UTC format');
  }
  const epochMs = Date.parse(value);
  if (!Number.isSafeInteger(epochMs) || epochMs < 0 || new Date(epochMs).toISOString() !== value) {
    throw new RequestError('clockStartUtc must be a valid UTC instant at or after 1970-01-01T00:00:00.000Z');
  }
  return epochMs;
}

function canonicalJson(value) {
  if (Array.isArray(value)) return `[${value.map(canonicalJson).join(',')}]`;
  if (isObject(value)) {
    return `{${Object.keys(value).sort().map((key) => `${JSON.stringify(key)}:${canonicalJson(value[key])}`).join(',')}}`;
  }
  return JSON.stringify(value);
}

function validateRequest(value) {
  assertObject(value, 'request');
  assertKnownFields(value, new Set([
    'schemaVersion', 'effectId', 'expectedEffectName', 'paletteId', 'brightness', 'speed', 'scale', 'seed',
    'clockStartUtc', 'atMs', 'views', 'previewScale',
  ]), 'request');

  if (value.schemaVersion !== 1) throw new RequestError('schemaVersion must be 1');
  assertInteger(value.effectId, 'effectId', 0, 255);
  if (value.expectedEffectName !== undefined && (typeof value.expectedEffectName !== 'string' || !value.expectedEffectName)) {
    throw new RequestError('expectedEffectName must be a non-empty string when present');
  }
  if (value.paletteId !== undefined) assertInteger(value.paletteId, 'paletteId', 0, 255);
  for (const key of ['brightness', 'speed', 'scale']) {
    if (value[key] !== undefined) assertInteger(value[key], key, 0, 255);
  }
  assertInteger(value.seed, 'seed', 1, 0xffffffff);
  const clockStartEpochMs = parseClockStartUtc(value.clockStartUtc);

  if (!Array.isArray(value.atMs) || value.atMs.length === 0 || value.atMs.length > MAX_CAPTURE_COUNT) {
    throw new RequestError(`atMs must contain 1..${MAX_CAPTURE_COUNT} timestamps`);
  }
  let previous = -1;
  for (const atMs of value.atMs) {
    assertInteger(atMs, 'atMs value', 0, 0xffffffff);
    if (atMs > MAX_SIMULATION_MS) throw new RequestError(`atMs exceeds maxSimulationMs ${MAX_SIMULATION_MS}`);
    if (atMs <= previous) throw new RequestError('atMs must be strictly sorted and unique');
    previous = atMs;
  }
  if (!Array.isArray(value.views) || value.views.length === 0 || value.views.length > VIEW_NAMES.size) {
    throw new RequestError('views must be a non-empty unique list');
  }
  const seenViews = new Set();
  for (const view of value.views) {
    if (!VIEW_NAMES.has(view) || seenViews.has(view)) throw new RequestError('views contains an unsupported or duplicate view');
    seenViews.add(view);
  }
  const previewScale = value.previewScale === undefined ? 16 : assertInteger(value.previewScale, 'previewScale', 1, MAX_PREVIEW_SCALE);

  return {
    ...value,
    clockStartEpochMs,
    previewScale,
  };
}

function verifyWorkLimits(validated, frameMs) {
  assertInteger(frameMs, 'sim_job_frame_ms()', 1, 0xffffffff);
  const projectedInternalSteps = Math.floor(validated.atMs[validated.atMs.length - 1] / frameMs) + 1;
  if (projectedInternalSteps > MAX_INTERNAL_STEPS) {
    throw new RequestError(`projected internal steps ${projectedInternalSteps} exceeds maxInternalSteps ${MAX_INTERNAL_STEPS}`);
  }
  return { ...validated, frameMs, projectedInternalSteps };
}

function resolveRequest(validated, catalog) {
  const effect = catalog.effects.find((entry) => entry.id === validated.effectId);
  if (!effect) throw new RequestError(`unknown effectId ${validated.effectId}`);
  if (validated.expectedEffectName !== undefined && validated.expectedEffectName !== effect.name) {
    throw new RequestError(`expectedEffectName ${JSON.stringify(validated.expectedEffectName)} does not match ${JSON.stringify(effect.name)}`);
  }
  const paletteId = validated.paletteId === undefined ? 0 : validated.paletteId;
  const palette = catalog.palettes.find((entry) => entry.id === paletteId);
  if (!palette) throw new RequestError(`unknown paletteId ${paletteId}`);

  return {
    schemaVersion: 1,
    effectId: effect.id,
    effectName: effect.name,
    paletteId: palette.id,
    paletteName: palette.name,
    brightness: validated.brightness === undefined ? effect.settings.brightness : validated.brightness,
    speed: validated.speed === undefined ? effect.settings.speed : validated.speed,
    scale: validated.scale === undefined ? effect.settings.scale : validated.scale,
    seed: validated.seed,
    clockStartUtc: validated.clockStartUtc,
    clockStartEpochMs: validated.clockStartEpochMs,
    atMs: validated.atMs,
    views: validated.views,
    previewScale: validated.previewScale,
    limits: {
      maxCaptureCount: MAX_CAPTURE_COUNT,
      maxInternalSteps: MAX_INTERNAL_STEPS,
      maxSimulationMs: MAX_SIMULATION_MS,
      projectedInternalSteps: validated.projectedInternalSteps,
      frameMs: validated.frameMs,
    },
  };
}

module.exports = {
  MAX_CAPTURE_COUNT,
  MAX_INTERNAL_STEPS,
  MAX_SIMULATION_MS,
  RequestError,
  canonicalJson,
  resolveRequest,
  validateRequest,
  verifyWorkLimits,
};
