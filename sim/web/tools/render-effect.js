#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');

const { renderEffect } = require('./lib/export-job');
const { catalogFromModule, loadWasm } = require('./lib/wasm-loader');

function usage() {
  return 'Usage: render:catalog --json | render:effect --request <path|-> --out <path>';
}

function readRequest(source) {
  const text = source === '-' ? fs.readFileSync(0, 'utf8') : fs.readFileSync(path.resolve(source), 'utf8');
  try {
    return JSON.parse(text);
  } catch (error) {
    throw new Error(`request is not valid JSON: ${error.message}`);
  }
}

function parseEffectArgs(args) {
  if (args.length !== 4 || args[0] !== '--request' || args[2] !== '--out' || !args[1] || !args[3]) throw new Error(usage());
  return { request: args[1], out: args[3] };
}

async function main(args) {
  const command = args.shift();
  if (command === 'catalog') {
    if (args.length !== 1 || args[0] !== '--json') throw new Error(usage());
    const { module } = await loadWasm();
    return { command: 'render:catalog', catalog: catalogFromModule(module) };
  }
  if (command === 'effect') {
    const options = parseEffectArgs(args);
    return { command: 'render:effect', ...(await renderEffect(readRequest(options.request), options.out)) };
  }
  throw new Error(usage());
}

main(process.argv.slice(2)).then(
  (result) => process.stdout.write(`${JSON.stringify(result)}\n`),
  (error) => {
    process.stderr.write(`render: ${error.message}\n`);
    process.exitCode = 1;
  },
);
