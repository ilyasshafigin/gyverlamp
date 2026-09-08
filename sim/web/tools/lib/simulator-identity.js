'use strict';

const childProcess = require('child_process');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const REPOSITORY_ROOT = path.resolve(__dirname, '../../../..');
const IDENTITY_PATH = path.join(REPOSITORY_ROOT, 'sim/web/public/wasm/gyverlamp_sim_wasm.identity.json');
const IDENTITY_SCHEMA_VERSION = 1;

function canonicalJson(value) {
  if (Array.isArray(value)) return `[${value.map(canonicalJson).join(',')}]`;
  if (value !== null && typeof value === 'object') {
    return `{${Object.keys(value).sort().map((key) => `${JSON.stringify(key)}:${canonicalJson(value[key])}`).join(',')}}`;
  }
  return JSON.stringify(value);
}

function sha256(value) {
  return crypto.createHash('sha256').update(value).digest('hex');
}

function commandVersion(command) {
  try {
    return childProcess.execFileSync(command, ['--version'], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).split('\n')[0].trim();
  } catch {
    return 'unavailable';
  }
}

function collectFiles(directory, files) {
  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    if (entry.name === '.DS_Store') continue;
    const absolute = path.join(directory, entry.name);
    const relative = path.relative(REPOSITORY_ROOT, absolute).split(path.sep).join('/');
    if (entry.isDirectory()) {
      if (relative === 'sim/wasm/build' || relative === 'sim/web/public' || relative.endsWith('/node_modules')) continue;
      collectFiles(absolute, files);
    } else if (entry.isFile()) {
      files.push(relative);
    }
  }
}

function sourceIdentity() {
  const files = [];
  for (const relative of ['sim/common', 'sim/host', 'sim/wasm', 'src']) collectFiles(path.join(REPOSITORY_ROOT, relative), files);
  files.push('platformio.ini', 'sim/CMakeLists.txt', 'sim/web/tools/lib/simulator-identity.js');
  files.sort();
  const entries = files.map((relative) => ({ relative, sha256: sha256(fs.readFileSync(path.join(REPOSITORY_ROOT, relative))) }));
  return { files: entries, sha256: sha256(canonicalJson(entries)) };
}

function buildProvenance() {
  return {
    cmake: commandVersion('cmake'),
    cmakeBuildType: process.env.CMAKE_BUILD_TYPE || '',
    cmakeGenerator: process.env.CMAKE_GENERATOR || '',
    emcc: commandVersion('emcc'),
    geometry: {
      connectionAngle: process.env.SIM_CONNECTION_ANGLE || '',
      height: process.env.SIM_HEIGHT || '',
      stripDirection: process.env.SIM_STRIP_DIRECTION || '',
      width: process.env.SIM_WIDTH || '',
    },
    target: 'gyverlamp_sim_wasm',
  };
}

function artifactHashes() {
  const artifacts = {
    glueJs: 'sim/web/public/wasm/gyverlamp_sim_wasm.js',
    wasm: 'sim/web/public/wasm/gyverlamp_sim_wasm.wasm',
  };
  for (const [key, relative] of Object.entries(artifacts)) {
    const absolute = path.join(REPOSITORY_ROOT, relative);
    if (!fs.existsSync(absolute)) throw new Error(`generated simulator artifact is missing: ${absolute}`);
    artifacts[key] = { path: relative, sha256: sha256(fs.readFileSync(absolute)) };
  }
  return artifacts;
}

function createIdentity() {
  const source = sourceIdentity();
  const identity = {
    artifacts: artifactHashes(),
    build: buildProvenance(),
    fileCount: source.files.length,
    schemaVersion: IDENTITY_SCHEMA_VERSION,
    sourceSha256: source.sha256,
  };
  return { ...identity, identitySha256: sha256(canonicalJson(identity)) };
}

function writeIdentity() {
  const identity = createIdentity();
  fs.mkdirSync(path.dirname(IDENTITY_PATH), { recursive: true });
  fs.writeFileSync(IDENTITY_PATH, `${canonicalJson(identity)}\n`);
  return identity;
}

function verifyIdentity() {
  if (!fs.existsSync(IDENTITY_PATH)) {
    throw new Error(`WASM simulator identity is missing. Run npm run build:wasm from sim/web; expected ${IDENTITY_PATH}`);
  }
  let recorded;
  try {
    recorded = JSON.parse(fs.readFileSync(IDENTITY_PATH, 'utf8'));
  } catch {
    throw new Error(`WASM simulator identity is invalid. Run npm run build:wasm from sim/web`);
  }
  const { identitySha256, ...payload } = recorded;
  if (identitySha256 !== sha256(canonicalJson(payload))) {
    throw new Error('WASM simulator identity is invalid. Run npm run build:wasm from sim/web');
  }
  const currentSource = sourceIdentity();
  if (recorded.schemaVersion !== IDENTITY_SCHEMA_VERSION || recorded.sourceSha256 !== currentSource.sha256) {
    throw new Error('WASM simulator is stale for current sources. Run npm run build:wasm from sim/web');
  }
  if (!recorded.artifacts || !recorded.artifacts.wasm || !recorded.artifacts.glueJs) {
    throw new Error('WASM simulator identity lacks generated artifact hashes. Run npm run build:wasm from sim/web');
  }
  for (const artifact of Object.values(recorded.artifacts)) {
    const absolute = path.join(REPOSITORY_ROOT, artifact.path);
    if (!fs.existsSync(absolute) || sha256(fs.readFileSync(absolute)) !== artifact.sha256) {
      throw new Error('WASM simulator binary or glue is stale. Run npm run build:wasm from sim/web');
    }
  }
  return recorded;
}

if (require.main === module) {
  if (process.argv.length !== 3 || process.argv[2] !== '--write') {
    process.stderr.write('Usage: simulator-identity.js --write\n');
    process.exitCode = 1;
  } else {
    process.stdout.write(`${JSON.stringify(writeIdentity())}\n`);
  }
}

module.exports = { IDENTITY_PATH, createIdentity, sourceIdentity, verifyIdentity, writeIdentity };
