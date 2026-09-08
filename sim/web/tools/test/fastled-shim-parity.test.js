'use strict';

const assert = require('assert');
const path = require('path');
const { execFileSync } = require('child_process');
const test = require('node:test');

test('FastLED shim matches committed lamp1_ota primitive golden', () => {
  const root = path.resolve(__dirname, '..', '..', '..', '..');
  const output = execFileSync('node', [path.join(root, 'sim', 'parity', 'run-parity.js')], { cwd: root, encoding: 'utf8' });
  assert.match(output, /FastLED shim parity golden: passed/);
});
