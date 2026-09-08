'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const { canonicalJson, resolveRequest, validateRequest, verifyWorkLimits } = require('./request');
const { DIFFUSER_PROFILE, RENDERER_VERSION, contactSheet, diffuse, encodePng, rgbaFromRgb, upscaleNearest } = require('./png-renderer');
const { catalogFromModule, loadWasm } = require('./wasm-loader');

const MANIFEST_VERSION = 1;
const MAX_MEMORY_BYTES = 128 * 1024 * 1024;
const PNG_ENCODE_BYTES_PER_PIXEL = 24;

function sha256(bytes) {
  return crypto.createHash('sha256').update(bytes).digest('hex');
}

function ensureEmptyDestination(destination) {
  if (fs.existsSync(destination)) throw new Error(`job output already exists: ${destination}`);
}

function writeFile(directory, relativePath, bytes) {
  const destination = path.join(directory, relativePath);
  fs.mkdirSync(path.dirname(destination), { recursive: true });
  fs.writeFileSync(destination, bytes);
}

function writeImage(directory, relativePath, image) {
  const png = encodePng(image);
  writeFile(directory, relativePath, png);
  return { path: relativePath, pngSha256: sha256(png), width: image.width, height: image.height };
}

function makeJobId(canonicalRequest, wasmHash, identityHash) {
  return sha256(`${canonicalRequest}\n${wasmHash}\n${identityHash}\n${RENDERER_VERSION}\n${canonicalJson(DIFFUSER_PROFILE)}`).slice(0, 24);
}

function contactSourceLevel(views) {
  if (views.includes('diffuser-v1')) return 'diffuser-v1';
  if (views.includes('sharp-v1')) return 'sharp-v1';
  return 'logical';
}

function estimateMemoryBytes(width, height, frameCount, previewScale, views) {
  const nativePixels = width * height;
  const previewPixels = nativePixels * previewScale * previewScale;
  const sourcePixels = contactSourceLevel(views) === 'logical' ? nativePixels : previewPixels;
  const contactSheetPixels = sourcePixels * 4 * Math.ceil(frameCount / 4);
  // Each PNG charge includes the Uint8Array RGBA source, Buffer.from copy,
  // pngjs filter/deflate chunks, result buffer and Buffer.concat copy. Charge
  // every frame allocation because GC timing is not an admission guarantee.
  const logicalBytes = views.includes('logical') ? (4 + PNG_ENCODE_BYTES_PER_PIXEL) * nativePixels : 0;
  const sharpBytes = views.includes('sharp-v1') ? (4 + PNG_ENCODE_BYTES_PER_PIXEL) * previewPixels : 0;
  const diffuserBytes = views.includes('diffuser-v1') ? (4 + PNG_ENCODE_BYTES_PER_PIXEL) * previewPixels : 0;
  // linear + two nested Gaussian passes + bloom passes + native sRGB RGBA.
  const diffusionIntermediates = views.includes('diffuser-v1') ? 52 * nativePixels : 0;
  const capturedRgb = 3 * nativePixels;
  const contactSheet = (4 + PNG_ENCODE_BYTES_PER_PIXEL) * contactSheetPixels;
  return frameCount * (capturedRgb + logicalBytes + sharpBytes + diffuserBytes + diffusionIntermediates) + contactSheet;
}

function readFrame(module, framebuffer, expectedSize) {
  const pointer = framebuffer();
  if (!pointer) throw new Error('WASM returned no job framebuffer');
  const result = new Uint8Array(module.HEAPU8.subarray(pointer, pointer + expectedSize));
  if (result.length !== expectedSize) throw new Error('WASM framebuffer is shorter than declared geometry');
  return result;
}

async function renderEffect(requestValue, outPath) {
  const validated = validateRequest(requestValue);
  const { module, wasmBytes, identity } = await loadWasm();
  const frameMs = module.cwrap('sim_job_frame_ms', 'number', [])();
  const limited = verifyWorkLimits(validated, frameMs);
  const catalog = catalogFromModule(module);
  const request = resolveRequest(limited, catalog);
  const canonicalRequest = canonicalJson(request);
  const wasmHash = sha256(wasmBytes);
  const jobId = makeJobId(canonicalRequest, wasmHash, identity.identitySha256);
  const outputRoot = path.resolve(outPath);
  const destination = path.join(outputRoot, jobId);
  ensureEmptyDestination(destination);
  fs.mkdirSync(outputRoot, { recursive: true });
  const temporary = path.join(outputRoot, `.${jobId}.tmp-${process.pid}`);
  if (fs.existsSync(temporary)) fs.rmSync(temporary, { recursive: true, force: true });
  fs.mkdirSync(temporary, { recursive: false });

  try {
    const init = module.cwrap('sim_job_init', 'number', ['number', 'number', 'number', 'number', 'number', 'number', 'number']);
    const advance = module.cwrap('sim_job_advance_to', 'number', ['number']);
    const framebuffer = module.cwrap('sim_job_framebuffer', 'number', []);
    const framebufferSize = module.cwrap('sim_job_framebuffer_size', 'number', []);
    const outputBrightness = module.cwrap('sim_job_output_brightness', 'number', []);
    const width = module.cwrap('sim_width', 'number', [])();
    const height = module.cwrap('sim_height', 'number', [])();
    if (!Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1) throw new Error('WASM returned invalid geometry');
    const memoryBytes = estimateMemoryBytes(width, height, request.atMs.length, request.previewScale, request.views);
    if (memoryBytes > MAX_MEMORY_BYTES) {
      throw new Error(`requested output exceeds memory budget ${MAX_MEMORY_BYTES} bytes (estimated ${memoryBytes})`);
    }
    if (!init(request.effectId, request.paletteId, request.brightness, request.speed, request.scale, request.seed, request.clockStartEpochMs)) {
      throw new Error('sim_job_init failed; use one fresh WASM module per export job');
    }
    const expectedSize = width * height * 3;
    if (framebufferSize() !== expectedSize) throw new Error('WASM framebuffer size does not match geometry');
    const frames = [];
    const contactImages = [];
    for (let index = 0; index < request.atMs.length; index += 1) {
      const atMs = request.atMs[index];
      if (!advance(atMs)) throw new Error(`sim_job_advance_to rejected ${atMs}`);
      const rgb = readFrame(module, framebuffer, expectedSize);
      const brightness = outputBrightness();
      if (!Number.isInteger(brightness) || brightness < 0 || brightness > 255) throw new Error('WASM returned invalid applied output brightness');
      const prefix = `frames/${String(index).padStart(3, '0')}-${atMs}ms`;
      const images = [];
      const sourceLevel = contactSourceLevel(request.views);
      let contactImage;
      if (request.views.includes('logical')) {
        const image = { width, height, data: rgbaFromRgb(rgb) };
        const relativePath = `${prefix}.logical.png`;
        images.push({ level: 'logical', ...writeImage(temporary, relativePath, image) });
        if (sourceLevel === 'logical') contactImage = image;
      }
      if (request.views.includes('sharp-v1')) {
        const sharp = upscaleNearest(rgbaFromRgb(rgb, brightness), width, height, request.previewScale);
        const relativePath = `${prefix}.sharp-v1.png`;
        images.push({ level: 'sharp-v1', ...writeImage(temporary, relativePath, sharp) });
        if (sourceLevel === 'sharp-v1') contactImage = sharp;
      }
      if (request.views.includes('diffuser-v1')) {
        const diffused = diffuse(rgb, width, height, request.previewScale, brightness);
        const relativePath = `${prefix}.diffuser-v1.png`;
        images.push({ level: 'diffuser-v1', ...writeImage(temporary, relativePath, diffused) });
        contactImage = diffused;
      }
      contactImages.push(contactImage);
      frames.push({
        atMs,
        appliedOutputBrightness: brightness,
        rgbSha256: sha256(rgb),
        images,
      });
    }
    const sheet = contactSheet(contactImages);
    const sheetPath = 'contact-sheet.png';
    const contactSheetInfo = writeImage(temporary, sheetPath, sheet);
    const manifest = {
      manifestVersion: MANIFEST_VERSION,
      jobId,
      canonicalRequest,
      request,
      engine: { identity, wasmSha256: wasmHash },
      geometry: { width, height, rowOrder: 'bottom-row-first' },
      scheduler: { frameMs, captureReads: 'last-displayed-frame' },
      outputBudget: { estimatedBytes: memoryBytes, maxBytes: MAX_MEMORY_BYTES },
      renderer: { version: RENDERER_VERSION, colorProfile: { sharp: 'sRGB byte scale', diffuser: DIFFUSER_PROFILE } },
      frames,
      contactSheet: {
        ...contactSheetInfo,
        columns: sheet.columns,
        frameOrder: request.atMs,
        rows: sheet.rows,
        sourceLevel: contactSourceLevel(request.views),
      },
      fidelityWarnings: [
        'Simulator FastLED output approximates ESP8266/device output.',
        'diffuser-v1 is an approximate CPU optical profile, not hardware calibration.',
        'Current limiting is not modeled in export previews.',
        'Effects that deliberately call randomSeed() can override request seed.',
      ],
    };
    writeFile(temporary, 'manifest.json', `${canonicalJson(manifest)}\n`);
    fs.renameSync(temporary, destination);
    return { jobId, manifest: path.relative(outputRoot, path.join(destination, 'manifest.json')), output: path.relative(outputRoot, destination) };
  } catch (error) {
    fs.rmSync(temporary, { recursive: true, force: true });
    throw error;
  }
}

module.exports = { MANIFEST_VERSION, MAX_MEMORY_BYTES, estimateMemoryBytes, makeJobId, renderEffect, sha256 };
