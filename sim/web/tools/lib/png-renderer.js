'use strict';

const { PNG } = require('pngjs');

const RENDERER_VERSION = 'sim-effect-export-v1';
const DIFFUSER_PROFILE = {
  id: 'diffuser-v1',
  linearRgb: true,
  ledPitchUnits: 'LED pitch',
  blurRadiusLedPitch: 2,
  horizontalEdge: 'wrap',
  verticalEdge: 'clamp',
  gaussianPasses: 2,
  kernel: [1, 4, 6, 4, 1],
  bloom: 0.18,
  exposure: 1,
  srgbLutEntries: 4096,
};

function roundByte(value) {
  return Math.max(0, Math.min(255, Math.floor(value + 0.5)));
}

function scaleByte(value, brightness) {
  return Math.floor((value * brightness + 127) / 255);
}

function rgbaFromRgb(rgb, brightness = 255) {
  const output = new Uint8Array((rgb.length / 3) * 4);
  for (let source = 0, target = 0; source < rgb.length; source += 3, target += 4) {
    output[target] = scaleByte(rgb[source], brightness);
    output[target + 1] = scaleByte(rgb[source + 1], brightness);
    output[target + 2] = scaleByte(rgb[source + 2], brightness);
    output[target + 3] = 255;
  }
  return output;
}

function upscaleNearest(rgba, width, height, scale) {
  const output = new Uint8Array(width * scale * height * scale * 4);
  const outputWidth = width * scale;
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      const source = (y * width + x) * 4;
      for (let dy = 0; dy < scale; dy += 1) {
        for (let dx = 0; dx < scale; dx += 1) {
          const target = ((y * scale + dy) * outputWidth + x * scale + dx) * 4;
          output[target] = rgba[source];
          output[target + 1] = rgba[source + 1];
          output[target + 2] = rgba[source + 2];
          output[target + 3] = 255;
        }
      }
    }
  }
  return { data: output, width: outputWidth, height: height * scale };
}

function makeSrgbEncodeLut() {
  const encode = new Uint8Array(DIFFUSER_PROFILE.srgbLutEntries);
  for (let index = 0; index < encode.length; index += 1) {
    const value = index / (encode.length - 1);
    const srgb = value <= 0.0031308 ? value * 12.92 : 1.055 * Math.pow(value, 1 / 2.4) - 0.055;
    encode[index] = roundByte(srgb * 255);
  }
  return encode;
}

const SRGB_ENCODE = makeSrgbEncodeLut();

function blurPass(input, width, height, horizontal) {
  const output = new Float32Array(input.length);
  const kernel = DIFFUSER_PROFILE.kernel;
  const weight = 16;
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      for (let channel = 0; channel < 3; channel += 1) {
        let sum = 0;
        for (let offset = -2; offset <= 2; offset += 1) {
          const sourceX = horizontal ? (x + offset + width) % width : x;
          const sourceY = horizontal ? y : Math.max(0, Math.min(height - 1, y + offset));
          sum += input[(sourceY * width + sourceX) * 3 + channel] * kernel[offset + 2];
        }
        output[(y * width + x) * 3 + channel] = sum / weight;
      }
    }
  }
  return output;
}

function diffuse(logicalRgb, logicalWidth, logicalHeight, scale, brightness) {
  const pixels = logicalWidth * logicalHeight;
  const linear = new Float32Array(pixels * 3);
  const brightnessFactor = brightness / 255;
  for (let source = 0; source < logicalRgb.length; source += 3) {
    linear[source] = logicalRgb[source] / 255 * brightnessFactor;
    linear[source + 1] = logicalRgb[source + 1] / 255 * brightnessFactor;
    linear[source + 2] = logicalRgb[source + 2] / 255 * brightnessFactor;
  }
  const first = blurPass(blurPass(linear, logicalWidth, logicalHeight, true), logicalWidth, logicalHeight, false);
  const bloom = blurPass(blurPass(first, logicalWidth, logicalHeight, true), logicalWidth, logicalHeight, false);
  const data = new Uint8Array(pixels * 4);
  for (let pixel = 0; pixel < pixels; pixel += 1) {
    for (let channel = 0; channel < 3; channel += 1) {
      const value = Math.min(1, (first[pixel * 3 + channel] + bloom[pixel * 3 + channel] * DIFFUSER_PROFILE.bloom) * DIFFUSER_PROFILE.exposure);
      data[pixel * 4 + channel] = SRGB_ENCODE[Math.min(SRGB_ENCODE.length - 1, Math.floor(value * (SRGB_ENCODE.length - 1) + 0.5))];
    }
    data[pixel * 4 + 3] = 255;
  }
  return upscaleNearest(data, logicalWidth, logicalHeight, scale);
}

function encodePng(image) {
  return PNG.sync.write({ width: image.width, height: image.height, data: Buffer.from(image.data) }, {
    bitDepth: 8,
    colorType: 6,
    deflateLevel: 9,
    deflateStrategy: 3,
    filterType: 4,
    inputHasAlpha: true,
  });
}

function contactSheet(images, columns = 4) {
  if (images.length === 0) throw new Error('contact sheet requires at least one image');
  const cellWidth = images[0].width;
  const cellHeight = images[0].height;
  if (images.some((image) => image.width !== cellWidth || image.height !== cellHeight)) throw new Error('contact sheet image dimensions differ');
  const rows = Math.ceil(images.length / columns);
  const data = new Uint8Array(cellWidth * columns * cellHeight * rows * 4);
  for (let pixel = 3; pixel < data.length; pixel += 4) data[pixel] = 255;
  images.forEach((image, index) => {
    const offsetX = (index % columns) * cellWidth;
    const offsetY = Math.floor(index / columns) * cellHeight;
    for (let y = 0; y < cellHeight; y += 1) {
      data.set(image.data.subarray(y * cellWidth * 4, (y + 1) * cellWidth * 4), ((offsetY + y) * cellWidth * columns + offsetX) * 4);
    }
  });
  return { data, width: cellWidth * columns, height: cellHeight * rows, columns, rows };
}

module.exports = {
  DIFFUSER_PROFILE,
  RENDERER_VERSION,
  contactSheet,
  diffuse,
  encodePng,
  rgbaFromRgb,
  scaleByte,
  upscaleNearest,
};
