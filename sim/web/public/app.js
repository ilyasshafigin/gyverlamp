const canvas = document.getElementById('matrix');
const ctx = canvas.getContext('2d');
const connectionBadge = document.getElementById('connectionStatus');
const connectionText = connectionBadge.querySelector('.text');
const statusDetail = document.getElementById('statusDetail');
const frameInfo = document.getElementById('frameInfo');
const lastFrameAge = document.getElementById('lastFrameAge');
const renderInfo = document.getElementById('renderInfo');
const stageInfo = document.getElementById('stageInfo');
const pauseBtn = document.getElementById('pause');
const stepBtn = document.getElementById('step');
const resetBtn = document.getElementById('resetDefaults');
const audioMicToggleBtn = document.getElementById('audioMicToggle');
const audioControls = document.getElementById('audioControls');
const audioStatusValue = document.getElementById('audioStatusValue');
const audioModeSelect = document.getElementById('audioMode');
const audioBandSelect = document.getElementById('audioBand');
const audioAmountInput = document.getElementById('audioAmount');
const audioAmountValue = document.getElementById('audioAmountValue');
const micGainInput = document.getElementById('micGain');
const micGainValue = document.getElementById('micGainValue');
const audioLevelValue = document.getElementById('audioLevelValue');
const audioBassValue = document.getElementById('audioBassValue');
const audioTrebleValue = document.getElementById('audioTrebleValue');
const audioAvailableValue = document.getElementById('audioAvailableValue');
const audioUnderrunsValue = document.getElementById('audioUnderrunsValue');
const audioOverflowsValue = document.getElementById('audioOverflowsValue');
const diffuserSharpBtn = document.getElementById('diffuserSharp');
const diffuserDiffusedBtn = document.getElementById('diffuserDiffused');
const diffuserCylinderBtn = document.getElementById('diffuserCylinder');
const cylinderCanvasEl = document.getElementById('cylinderCanvas');
const blurRow = document.getElementById('blurRow');
const bloomRow = document.getElementById('bloomRow');
const blurInput = document.getElementById('diffuserBlur');
const bloomInput = document.getElementById('diffuserBloom');
const firmwareVersionBadge = document.getElementById('firmwareVersion');
const simVersionBadge = document.getElementById('simVersion');

let width = 16;
let height = 16;
let imageData = null;
const diffuserCellSize = 24;
const diffuserMaxBlurPx = 48;
const SHADE_RATIO = 2.1;
const FIXED_LED_GAP = 0;
const FIXED_MILKINESS = 0;
const FIXED_PLASTIC = 100;
const diffuserSource = document.createElement('canvas');
const diffuserSourceCtx = diffuserSource.getContext('2d');
const diffuserBloomSource = document.createElement('canvas');
const diffuserBloomSourceCtx = diffuserBloomSource.getContext('2d');
const userAgent = navigator.userAgent || '';
const isSafari = /^((?!chrome|android|crios|fxios).)*safari/i.test(userAgent);
const useCanvasFilter = 'filter' in ctx && !isSafari;
const preview = {
  renderMode: 'sharp',
  diffuser: false,
  blur: 14,
  bloom: 80,
};
const FIXED_FPS = 60;
let currentBrightness = 255;

let frameCount = 0;
let currentFps = null;
let fpsStartedAt = performance.now();
let lastFrameAt = null;
let lastFrame = null;
let renderFw = 0;
let renderFh = 0;

let currentEffectId = null;
let effectsList = [];
let currentPaletteId = null;
let palettesList = [];
let currentEffectName = '—';
let currentPaletteName = '—';
let configApplied = false;
let paused = false;
const storageKey = 'gyverlamp-sim-ui';
const MIC_ADC_PREAMP = 4;
const URL_STATE_KEYS = ['effect', 'palette', 'brightness', 'speed', 'scale'];
const URL_STATE_KEY_SET = new Set(URL_STATE_KEYS);
let urlStateTimer = null;
const audioUi = {
  starting: false,
  everEnabled: false,
  telemetry: null,
};

let activeRunner = null;

const THROTTLE_MS = 80;
const pendingSends = new Map();

function readStoredState() {
  try {
    return JSON.parse(localStorage.getItem(storageKey) || '{}') || {};
  } catch {
    return {};
  }
}

function readUrlState() {
  const hash = location.hash.startsWith('#') ? location.hash.slice(1) : '';
  const params = new URLSearchParams(hash);
  const state = {};
  for (const key of URL_STATE_KEYS) {
    if (!params.has(key)) continue;
    const value = parseInt(params.get(key), 10);
    if (Number.isFinite(value)) state[key] = value;
  }
  return state;
}

const storedState = { ...readStoredState(), ...readUrlState() };

function replaceUrlState() {
  const params = new URLSearchParams();
  for (const key of URL_STATE_KEYS) {
    if (!hasStored(key)) continue;
    const value = parseInt(storedState[key], 10);
    if (Number.isFinite(value)) params.set(key, String(value));
  }
  const hash = params.toString();
  const nextUrl = `${location.pathname}${location.search}${hash ? `#${hash}` : ''}`;
  history.replaceState(null, '', nextUrl);
}

function scheduleUrlStateUpdate() {
  if (urlStateTimer) clearTimeout(urlStateTimer);
  urlStateTimer = setTimeout(() => {
    urlStateTimer = null;
    replaceUrlState();
  }, 150);
}

function saveState(key, value) {
  storedState[key] = value;
  try {
    localStorage.setItem(storageKey, JSON.stringify(storedState));
  } catch {
    // ignore storage failures
  }
  if (URL_STATE_KEY_SET.has(key)) scheduleUrlStateUpdate();
}

function hasStored(key) {
  return Object.prototype.hasOwnProperty.call(storedState, key);
}

function findEffect(id) {
  return effectsList.find((e) => Number(e.id) === Number(id));
}

function setConnection(state, detail) {
  connectionBadge.className = `status-badge ${state}`;
  connectionText.textContent = detail || state;
}

function setSlider(type, value) {
  const el = document.getElementById(type);
  const valEl = document.getElementById(type + 'Value');
  if (el) el.value = value;
  if (valEl) valEl.textContent = value;
  if (type === 'brightness') {
    currentBrightness = clampByte(value);
    rerenderLastFrame();
  }
}

function clampByte(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) return 0;
  return Math.max(0, Math.min(255, n));
}

function applyBrightness(value) {
  return Math.round(clampByte(value) * currentBrightness / 255);
}

function diffuserBackgroundColor() {
  const level = FIXED_PLASTIC;
  const channel = Math.round(level * 255 / 100);
  return `rgb(${channel}, ${channel}, ${Math.max(0, channel - 4)})`;
}

function clampPercent(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) return 0;
  return Math.max(0, Math.min(100, n));
}

function clampMicGain(value) {
  const n = Number(value);
  if (!Number.isFinite(n)) return 100;
  return Math.max(25, Math.min(400, Math.round(n)));
}

function setSelectValue(el, value) {
  if (el) el.value = String(value);
}

function setText(el, value) {
  if (el) el.textContent = value;
}

function formatAudioTelemetryValue(value) {
  if (value === null || value === undefined || Number.isNaN(value)) return '—';
  return String(Math.round(Number(value)));
}

function formatAudioStatus(telemetry) {
  if (audioUi.starting) return { text: 'Starting…', state: 'starting' };
  if (!configApplied || !activeRunner || !activeRunner.mic) return { text: 'Loading…', state: 'loading' };
  if (!telemetry) return { text: 'Stopped', state: 'stopped' };

  const error = String(telemetry.error || '').trim();
  const lower = error.toLowerCase();
  if (telemetry.status === 'running' && telemetry.running) return { text: 'Running', state: 'running' };
  if (telemetry.status === 'error') {
    if (lower.includes('unsupported')) return { text: 'Unsupported', state: 'error' };
    if (lower.includes('denied') || lower.includes('permission') || lower.includes('secure') || lower.includes('https')) {
      return { text: 'Permission denied', state: 'error' };
    }
    return { text: error ? `Error: ${error}` : 'Error', state: 'error' };
  }
  if (telemetry.status === 'starting') return { text: 'Starting…', state: 'starting' };
  if (telemetry.status === 'stopped') return { text: 'Stopped', state: 'stopped' };
  return { text: 'Stopped', state: 'stopped' };
}

function refreshAudioUi(telemetry) {
  const snapshot = telemetry === undefined ? readAudioTelemetry() : telemetry;
  audioUi.telemetry = snapshot;
  const status = formatAudioStatus(snapshot);

  setText(audioStatusValue, status.text);
  audioMicToggleBtn.disabled = !!audioUi.starting || !configApplied || !activeRunner || !activeRunner.mic;
  audioMicToggleBtn.textContent = status.state === 'running'
    ? 'Disable mic'
    : audioUi.starting
      ? 'Starting…'
      : (!configApplied || !activeRunner || !activeRunner.mic)
        ? 'Loading…'
        : 'Enable mic';

  if (audioControls) {
    audioControls.hidden = status.state !== 'running' && status.state !== 'starting';
  }

  if (snapshot) {
    setText(audioLevelValue, formatAudioTelemetryValue(snapshot.level));
    setText(audioBassValue, formatAudioTelemetryValue(snapshot.bass));
    setText(audioTrebleValue, formatAudioTelemetryValue(snapshot.treble));
    setText(audioAvailableValue, snapshot.available ? 'Yes' : 'No');
    setText(audioUnderrunsValue, formatAudioTelemetryValue(snapshot.underruns));
    setText(audioOverflowsValue, formatAudioTelemetryValue(snapshot.overflows));
  } else {
    setText(audioLevelValue, '—');
    setText(audioBassValue, '—');
    setText(audioTrebleValue, '—');
    setText(audioAvailableValue, '—');
    setText(audioUnderrunsValue, '—');
    setText(audioOverflowsValue, '—');
  }
}

function syncAudioMode(value, persist = true, sendToRunner = true) {
  const mode = Math.max(0, Math.min(4, Math.round(Number(value))));
  setSelectValue(audioModeSelect, mode);
  if (persist) saveState('audioMode', mode);
  if (sendToRunner) send('audioMode', mode);
  return mode;
}

function syncAudioBand(value, persist = true, sendToRunner = true) {
  const band = Math.max(0, Math.min(2, Math.round(Number(value))));
  setSelectValue(audioBandSelect, band);
  if (persist) saveState('audioBand', band);
  if (sendToRunner) send('audioBand', band);
  return band;
}

function syncAudioAmount(value, persist = true, sendToRunner = true) {
  const amount = clampByte(value);
  setText(audioAmountValue, String(amount));
  audioAmountInput.value = String(amount);
  if (persist) saveState('audioAmount', amount);
  if (sendToRunner) scheduleSend('audioAmount', amount);
  return amount;
}

function syncMicGain(value, persist = true, sendToRunner = true) {
  const gain = clampMicGain(value);
  setText(micGainValue, `${gain}%`);
  micGainInput.value = String(gain);
  if (persist) saveState('micGain', gain);
  if (sendToRunner) scheduleSend('audioGain', gain);
  return gain;
}

function readAudioTelemetry() {
  if (!activeRunner || !activeRunner.mic) return null;
  try {
    return activeRunner.send('audioTelemetry') || null;
  } catch {
    return null;
  }
}

async function toggleAudioMic() {
  if (!configApplied || !activeRunner || !activeRunner.mic || audioUi.starting) return;

  const snapshot = readAudioTelemetry();
  if (snapshot && snapshot.running) {
    activeRunner.send('audioMicStop');
    audioUi.starting = false;
    refreshAudioUi(readAudioTelemetry());
    return;
  }

  audioUi.starting = true;
  refreshAudioUi(snapshot);

  const ok = await activeRunner.send('audioMicStart');
  audioUi.starting = false;

  const nextTelemetry = readAudioTelemetry();
  refreshAudioUi(nextTelemetry);

  if (!ok) return;

  if (!audioUi.everEnabled) {
    audioUi.everEnabled = true;
    if (Number(audioModeSelect.value) === 0) {
      syncAudioMode(4);
    }
  }

  refreshAudioUi(nextTelemetry);
}

function send(type, value) {
  if (activeRunner) activeRunner.send(type, value);
}

function scheduleSend(type, value) {
  const existing = pendingSends.get(type);
  if (existing) clearTimeout(existing.timer);
  pendingSends.set(type, {
    value,
    timer: setTimeout(() => {
      send(type, value);
      pendingSends.delete(type);
    }, THROTTLE_MS),
  });
}

function flushSend(type, value) {
  const existing = pendingSends.get(type);
  if (existing) clearTimeout(existing.timer);
  pendingSends.delete(type);
  send(type, value);
}

function clearPendingSends() {
  for (const entry of pendingSends.values()) {
    clearTimeout(entry.timer);
  }
  pendingSends.clear();
}

function bindControl(id, type) {
  const el = document.getElementById(id);
  const valEl = document.getElementById(id + 'Value');
  el.addEventListener('input', () => {
    const value = parseInt(el.value, 10);
    if (valEl) valEl.textContent = el.value;
    if (type === 'brightness') {
      currentBrightness = clampByte(value);
      rerenderLastFrame();
    }
    saveState(type, value);
    scheduleSend(type, value);
  });
  el.addEventListener('change', () => {
    const value = parseInt(el.value, 10);
    if (valEl) valEl.textContent = el.value;
    if (type === 'brightness') {
      currentBrightness = clampByte(value);
      rerenderLastFrame();
    }
    saveState(type, value);
    flushSend(type, value);
  });
}

bindControl('brightness', 'brightness');
bindControl('speed', 'speed');
bindControl('scale', 'scale');

function bindAudioSelect(el, storageKey, syncFn) {
  el.addEventListener('change', () => {
    const value = parseInt(el.value, 10);
    syncFn(value, true, true);
  });
  if (hasStored(storageKey)) {
    syncFn(storedState[storageKey], false, false);
  }
}

function bindAudioRange(el, valueEl, storageKey, syncFn, suffix = '') {
  const update = (persist = true, sendToRunner = true) => {
    const value = parseInt(el.value, 10);
    syncFn(value, persist, sendToRunner);
    if (valueEl) valueEl.textContent = `${el.value}${suffix}`;
  };
  el.addEventListener('input', () => update(true, true));
  el.addEventListener('change', () => update(true, true));
  if (hasStored(storageKey)) el.value = String(storedState[storageKey]);
  update(false, false);
}

bindAudioSelect(audioModeSelect, 'audioMode', syncAudioMode);
bindAudioSelect(audioBandSelect, 'audioBand', syncAudioBand);
bindAudioRange(audioAmountInput, audioAmountValue, 'audioAmount', syncAudioAmount);
bindAudioRange(micGainInput, micGainValue, 'micGain', syncMicGain, '%');

audioMicToggleBtn.addEventListener('click', () => {
  toggleAudioMic().catch((err) => {
    console.error(err);
    audioUi.starting = false;
    refreshAudioUi(readAudioTelemetry());
  });
});

function bindPreviewRange(id, key, suffix) {
  const el = document.getElementById(id);
  const valEl = document.getElementById(id + 'Value');
  const update = () => {
    preview[key] = parseInt(el.value, 10);
    if (valEl) valEl.textContent = `${el.value}${suffix}`;
    saveState(id, preview[key]);
    rerenderLastFrame();
  };
  if (hasStored(id)) el.value = storedState[id];
  el.addEventListener('input', update);
  update();
}

function bindDiffuserBlur() {
  const el = document.getElementById('diffuserBlur');
  const valEl = document.getElementById('diffuserBlurValue');
  const update = () => {
    const percent = clampPercent(parseInt(el.value, 10));
    preview.blur = Math.round(percent * diffuserMaxBlurPx / 100);
    if (valEl) valEl.textContent = `${percent}%`;
    saveState('diffuserBlur', percent);
    rerenderLastFrame();
  };
  if (hasStored('diffuserBlur')) el.value = storedState.diffuserBlur;
  el.addEventListener('input', update);
  update();
}

function updateRenderInfo() {
  const mode = preview.renderMode;
  const blurNote = mode === 'diffused' && preview.blur > 0 && !useCanvasFilter ? ' · css blur fallback' : '';
  const info = renderFw && renderFh ? `${renderFw}×${renderFh} · ${mode}${blurNote}` : `— · ${mode}${blurNote}`;
  renderInfo.textContent = info;
  stageInfo.textContent = info;
}

function setRenderMode(mode) {
  const m = mode === 'diffused' || mode === 'cylinder' ? mode : 'sharp';
  preview.renderMode = m;
  preview.diffuser = m === 'diffused';

  diffuserSharpBtn.classList.toggle('active', m === 'sharp');
  diffuserDiffusedBtn.classList.toggle('active', m === 'diffused');
  diffuserCylinderBtn.classList.toggle('active', m === 'cylinder');

  const useCylinder = m === 'cylinder';
  canvas.style.display = useCylinder ? 'none' : 'block';
  canvas.classList.toggle('diffuser', m === 'diffused');
  if (cylinderCanvasEl) {
    cylinderCanvasEl.style.display = useCylinder ? 'block' : 'none';
  }

  const slidersActive = m === 'diffused' || m === 'cylinder';
  blurRow.classList.toggle('dim', !slidersActive);
  bloomRow.classList.toggle('dim', !slidersActive);
  blurInput.disabled = !slidersActive;
  bloomInput.disabled = !slidersActive;

  if (m !== 'diffused') canvas.style.filter = 'none';

  saveState('renderMode', m);
  saveState('diffuser', preview.diffuser);

  if (useCylinder) {
    ensureCylinder();
    resizeCylinder();
  }

  rerenderLastFrame();
  updateRenderInfo();
}

diffuserSharpBtn.addEventListener('click', () => setRenderMode('sharp'));
diffuserDiffusedBtn.addEventListener('click', () => setRenderMode('diffused'));
diffuserCylinderBtn.addEventListener('click', () => setRenderMode('cylinder'));

const cylinder = {
  initialized: false,
  scene: null,
  camera: null,
  renderer: null,
  composer: null,
  bloomPass: null,
  postReady: false,
  postLoading: false,
  shadeMesh: null,
  texture: null,
  shadeTexture: null,
  canvas: null,
  ctx: null,
  shadeCanvas: null,
  shadeCtx: null,
  shadeWorkCanvas: null,
  shadeWorkCtx: null,
  geoW: 0,
  geoH: 0,
  yaw: -Math.PI / 2,
  dragging: false,
  lastX: 0,
  userZoom: 1,
  resizeObs: null,
};

function getInitialRenderMode() {
  const stored = storedState.renderMode;
  if (stored === 'sharp' || stored === 'diffused' || stored === 'cylinder') return stored;
  if (hasStored('diffuser')) return storedState.diffuser ? 'diffused' : 'sharp';
  return 'sharp';
}

setRenderMode(getInitialRenderMode());

bindDiffuserBlur();
bindPreviewRange('diffuserBloom', 'bloom', '%');

blurInput.addEventListener('input', updateRenderInfo);
bloomInput.addEventListener('input', updateRenderInfo);

function updateStatusDetail() {
  const pauseLine = paused
    ? `<br><span class="value" style="color:var(--accent-2)">Paused</span>`
    : '';
  statusDetail.innerHTML =
    `<span class="label">Effect:</span> <span class="value">${currentEffectName}</span><br>` +
    `<span class="label">Palette:</span> <span class="value">${currentPaletteName}</span>` +
    pauseLine;
}

function populateEffects(effects, current) {
  effectsList = effects || [];
  const select = document.getElementById('effect');
  select.innerHTML = '';
  for (const entry of effects) {
    const option = document.createElement('option');
    option.value = entry.id;
    option.textContent = entry.name;
    if (entry.id === current) option.selected = true;
    select.appendChild(option);
  }
  currentEffectId = current;
  const entry = findEffect(current);
  currentEffectName = entry ? entry.name : '—';
  updateStatusDetail();
}

function populatePalettes(palettes, current) {
  palettesList = palettes || [];
  const select = document.getElementById('palette');
  select.innerHTML = '';
  for (const entry of palettesList) {
    const option = document.createElement('option');
    option.value = entry.id;
    option.textContent = entry.name;
    if (entry.id === current) option.selected = true;
    select.appendChild(option);
  }
  currentPaletteId = current;
  const entry = palettesList.find((p) => Number(p.id) === Number(current));
  currentPaletteName = entry ? entry.name : '—';
  updateStatusDetail();
}

document.getElementById('effect').addEventListener('change', (e) => {
  const id = Number(e.target.value);
  const entry = findEffect(id);
  if (entry && entry.settings) {
    setSlider('brightness', entry.settings.brightness);
    setSlider('speed', entry.settings.speed);
    setSlider('scale', entry.settings.scale);
    saveState('brightness', entry.settings.brightness);
    saveState('speed', entry.settings.speed);
    saveState('scale', entry.settings.scale);
  }
  send('effect', id);
  saveState('effect', id);
  currentEffectId = id;
  currentEffectName = entry ? entry.name : '—';
  updateStatusDetail();
});

document.getElementById('palette').addEventListener('change', (e) => {
  const id = Number(e.target.value);
  send('palette', id);
  saveState('palette', id);
  currentPaletteId = id;
  const entry = palettesList.find((p) => Number(p.id) === id);
  currentPaletteName = entry ? entry.name : '—';
  updateStatusDetail();
});

function resetDefaults() {
  const entry = findEffect(currentEffectId);
  if (entry && entry.settings) {
    for (const key of ['brightness', 'speed', 'scale']) {
      const value = entry.settings[key];
      setSlider(key, value);
      saveState(key, value);
      flushSend(key, value);
    }
  }
  send('reset', 1);
}

resetBtn.addEventListener('click', resetDefaults);

document.getElementById('powerOn').addEventListener('click', () => send('power', 'on'));
document.getElementById('powerOff').addEventListener('click', () => send('power', 'off'));
document.getElementById('powerToggle').addEventListener('click', () => send('power', 'toggle'));
document.getElementById('effectNext').addEventListener('click', () => send('effectStep', 'next'));
document.getElementById('effectPrev').addEventListener('click', () => send('effectStep', 'prev'));

function sendNotifyText() {
  const input = document.getElementById('notifyTextInput');
  const text = (input.value || '').trim().slice(0, 160);
  if (!text) return;
  send('notifyText', text);
  input.value = '';
}
document.getElementById('notifyTextSend').addEventListener('click', sendNotifyText);
document.getElementById('notifyTextInput').addEventListener('keydown', (e) => {
  if (e.key === 'Enter') sendNotifyText();
});
document.getElementById('notifyUser').addEventListener('click', () => send('notifyUser', 'notify'));
document.getElementById('notifyWarning').addEventListener('click', () => send('notifyUser', 'warning'));
document.getElementById('notifyAlarm').addEventListener('click', () => send('notifyUser', 'alarm'));
document.getElementById('notifyClear').addEventListener('click', () => send('notifyClear', 1));

function enqueueButton(value, count) {
  const payload = { value };
  if (count !== undefined) payload.count = count;
  send('button', payload);
}

document.getElementById('btnPress').addEventListener('click', () => enqueueButton('press', 1));
document.getElementById('btnRelease').addEventListener('click', () => enqueueButton('release'));
document.getElementById('btnTap').addEventListener('click', () => enqueueButton('tap', 1));
document.getElementById('btnDouble').addEventListener('click', () => enqueueButton('double'));
document.getElementById('btnTriple').addEventListener('click', () => enqueueButton('triple'));
document.getElementById('btnHold').addEventListener('click', () => enqueueButton('hold'));
document.getElementById('btnStep').addEventListener('click', () => enqueueButton('step'));

function togglePause() {
  paused = !paused;
  pauseBtn.textContent = paused ? 'Resume' : 'Pause';
  stepBtn.disabled = !paused;
  updateStatusDetail();
  send('pause', paused ? 1 : 0);
}

pauseBtn.addEventListener('click', togglePause);

function sendStep() {
  if (paused) send('step', 1);
}

stepBtn.addEventListener('click', sendStep);

document.addEventListener('keydown', (e) => {
  if (e.repeat) return;
  const tag = e.target.tagName;
  const editable = e.target.isContentEditable;
  if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA' || tag === 'BUTTON' || tag === 'A' || editable) {
    return;
  }
  if (e.key === ' ' || e.code === 'Space') {
    e.preventDefault();
    togglePause();
  } else if (e.key === 'ArrowRight') {
    if (paused) {
      e.preventDefault();
      sendStep();
    }
  } else if (e.key === 'r' || e.key === 'R') {
    e.preventDefault();
    resetDefaults();
  }
});

function applyStoredStateAfterConfig() {
  if (configApplied) return;
  configApplied = true;

  if (hasStored('effect')) {
    const id = Number(storedState.effect);
    const entry = findEffect(id);
    if (entry) {
      document.getElementById('effect').value = String(id);
      if (entry.settings) {
        if (!hasStored('brightness')) setSlider('brightness', entry.settings.brightness);
        if (!hasStored('speed')) setSlider('speed', entry.settings.speed);
        if (!hasStored('scale')) setSlider('scale', entry.settings.scale);
      }
      if (id !== currentEffectId) send('effect', id);
      currentEffectId = id;
      currentEffectName = entry.name;
    }
  }

  if (hasStored('palette')) {
    const id = Number(storedState.palette);
    const palette = palettesList.find((p) => Number(p.id) === id);
    if (palette) {
      document.getElementById('palette').value = String(id);
      if (id !== currentPaletteId) send('palette', id);
      currentPaletteId = id;
      currentPaletteName = palette.name;
    }
  }

  for (const key of ['brightness', 'speed', 'scale']) {
    if (!hasStored(key)) continue;
    const value = parseInt(storedState[key], 10);
    if (!Number.isFinite(value)) continue;
    setSlider(key, value);
    send(key, value);
  }

  if (hasStored('audioMode')) syncAudioMode(storedState.audioMode, false, true);
  if (hasStored('audioBand')) syncAudioBand(storedState.audioBand, false, true);
  if (hasStored('audioAmount')) syncAudioAmount(storedState.audioAmount, false, true);
  if (hasStored('micGain')) syncMicGain(storedState.micGain, false, true);

  updateStatusDetail();
  refreshAudioUi();
  replaceUrlState();
}

function handleConfig(msg) {
  if (msg.width && msg.height) {
    width = msg.width;
    height = msg.height;
    canvas.width = width;
    canvas.height = height;
    imageData = ctx.createImageData(width, height);
  }
  populateEffects(msg.effects || [], msg.effect);
  populatePalettes(msg.palettes || [], msg.palette);
  handlePaused(!!msg.paused);
  if (msg.brightness !== undefined) setSlider('brightness', msg.brightness);
  if (msg.speed !== undefined) setSlider('speed', msg.speed);
  if (msg.scale !== undefined) setSlider('scale', msg.scale);
  if (msg.audioMode !== undefined) syncAudioMode(msg.audioMode, false, false);
  if (msg.audioBand !== undefined) syncAudioBand(msg.audioBand, false, false);
  if (msg.audioAmount !== undefined) syncAudioAmount(msg.audioAmount, false, false);
  if (msg.micGain !== undefined) syncMicGain(msg.micGain, false, false);
  applyStoredStateAfterConfig();
}

function handleSettings(msg) {
  for (const key of ['brightness', 'speed', 'scale']) {
    if (msg[key] !== undefined) {
      setSlider(key, msg[key]);
      saveState(key, msg[key]);
    }
  }
}

function handlePaused(value) {
  paused = !!value;
  pauseBtn.textContent = paused ? 'Resume' : 'Pause';
  stepBtn.disabled = !paused;
  updateStatusDetail();
}

function handleFrameData(data) {
  const buf = data instanceof Uint8Array ? data : new Uint8Array(data);
  if (buf.length < 16 || String.fromCharCode(buf[0], buf[1], buf[2], buf[3]) !== 'GYVF') return;

  const fw = buf[4] | (buf[5] << 8);
  const fh = buf[6] | (buf[7] << 8);
  const pixels = fw * fh;
  const offset = 16;
  if (buf.length < offset + pixels * 3) return;

  renderFw = fw;
  renderFh = fh;

  lastFrame = { buf, offset, pixels, fw, fh };
  renderFrame(buf, offset, pixels, fw, fh);

  lastFrameAt = Date.now();
  frameCount++;
  const now = performance.now();
  if (now - fpsStartedAt >= 1000) {
    currentFps = frameCount;
    frameCount = 0;
    fpsStartedAt = now;
  }
  updateRenderInfo();
}

function updateFrameStats() {
  lastFrameAt = Date.now();
  frameCount++;
  const now = performance.now();
  if (now - fpsStartedAt >= 1000) {
    currentFps = frameCount;
    frameCount = 0;
    fpsStartedAt = now;
  }
  updateRenderInfo();
}

function handleRgbFrame(buf, fw, fh) {
  const pixels = fw * fh;
  if (!buf || buf.length < pixels * 3) return;

  renderFw = fw;
  renderFh = fh;

  lastFrame = { buf, offset: 0, pixels, fw, fh };
  renderFrame(buf, 0, pixels, fw, fh);
  updateFrameStats();
}

function showWasmError(err) {
  statusDetail.innerHTML =
    '<span class="value" style="color:var(--danger)">WASM runner unavailable</span><br>' +
    '<span class="label">Build it from repo root:</span><br>' +
    '<code>npm run build:wasm --prefix sim/web</code><br>' +
    '<span class="label">Or from sim/web:</span><br>' +
    '<code>npm run build:wasm</code><br>' +
    '<span class="label">Then hard reload page.</span>';
  console.error(err);
}

function callbacks() {
  return {
    onStatus: setConnection,
    onConfig: handleConfig,
    onFrame: handleFrameData,
    onRgbFrame: handleRgbFrame,
    onSettings: handleSettings,
    onPaused: handlePaused,
  };
}

class WasmRunner {
  constructor(callbacks) {
    this.callbacks = callbacks;
    this.module = null;
    this.animationId = null;
    this.frameNo = 0;
    this.fps = FIXED_FPS;
    this.frameInterval = 1000 / this.fps;
    this.lastTickAt = 0;
    this.simTime = 0;
    this.maxCatchupFrames = 3;
    this.paused = false;
    this.closed = false;
    this.hidden = false;
    this.onVisibilityChange = () => {
      const wasHidden = this.hidden;
      this.hidden = document.hidden;
      // Becoming visible again: relaunch the loop. lastTickAt is stale, but
      // the resync guard in loop() handles that on the first tick.
      if (wasHidden && !this.hidden && !this.closed && this.animationId === null) {
        this.loop(performance.now());
      }
    };
    document.addEventListener('visibilitychange', this.onVisibilityChange);
    this.config = null;
    this.mic = null;
    this.rgbFrame = null;
    this.rgbFrameBytes = 0;
    this.init();
  }

  async init() {
    this.callbacks.onStatus('connecting', 'Loading WASM…');
    try {
      await loadWasmGlue();
      this.module = await GyverLampSimModule({
        locateFile: (path) => path.endsWith('.wasm') ? 'wasm/gyverlamp_sim_wasm.wasm' : `wasm/${path}`,
      });
      if (!this.call('sim_init')) throw new Error('sim_init failed');
      this.mic = new BrowserMicInput(this);
      this.config = this.buildConfig();
      this.callbacks.onConfig(this.config);
      this.callbacks.onStatus('connected', 'WASM runner');
      this.loop(performance.now());
    } catch (err) {
      this.callbacks.onStatus('error', 'WASM missing. Run npm run build:wasm');
      showWasmError(err);
    }
  }

  call(name, ...args) {
    if (!this.module) return 0;
    const fn = this.module[`_${name}`];
    if (typeof fn === 'function') return fn(...args);
    return this.module.ccall(name, 'number', args.map(() => 'number'), args);
  }

  str(name, index) {
    const ptr = this.call(name, index);
    return ptr ? this.module.UTF8ToString(ptr) : '';
  }

  buildConfig() {
    const effects = [];
    const effectCount = this.call('sim_effect_count');
    for (let i = 0; i < effectCount; i++) {
      effects.push({
        id: this.call('sim_effect_id_at', i),
        name: this.str('sim_effect_name_at', i),
        settings: {
          brightness: this.call('sim_effect_default_brightness', i),
          speed: this.call('sim_effect_default_speed', i),
          scale: this.call('sim_effect_default_scale', i),
          resetOnChange: this.call('sim_effect_reset_on_change', i),
        },
      });
    }
    const palettes = [];
    const paletteCount = this.call('sim_palette_count');
    for (let i = 0; i < paletteCount; i++) {
      palettes.push({ id: this.call('sim_palette_id_at', i), name: this.str('sim_palette_name_at', i) });
    }
    const firstEffect = effects[0] || { id: 0, settings: { brightness: 255, speed: 128, scale: 128 } };
    const firstPalette = palettes[0] || { id: 0 };
    return {
      effects,
      palettes,
      effect: firstEffect.id,
      palette: firstPalette.id,
      brightness: firstEffect.settings.brightness,
      speed: firstEffect.settings.speed,
      scale: firstEffect.settings.scale,
      fps: FIXED_FPS,
      audioMode: this.call('sim_audio_mode'),
      audioBand: this.call('sim_audio_band'),
      audioAmount: this.call('sim_audio_amount'),
      micGain: this.mic ? this.mic.gain() : 100,
      paused: false,
      width: this.call('sim_width'),
      height: this.call('sim_height'),
    };
  }

  loop(now) {
    if (this.closed) return;
    // Stop scheduling rAF while the tab is hidden — saves CPU/battery and
    // avoids browser throttling weirdness. Relaunch from onVisibilityChange.
    if (this.hidden) {
      this.animationId = null;
      return;
    }
    if (!this.paused) {
      if (this.lastTickAt === 0) {
        this.simTime = now;
        this.lastTickAt = now;
      }
      // Tab was backgrounded or a long stall happened: rAF gets throttled,
      // so `now` jumps far past lastTickAt. The catch-up loop below would
      // then run maxCatchupFrames every rAF for a long time, making the
      // sim render 2-3x faster until the debt clears. Drop the debt and
      // resync the virtual clock to wall time instead.
      if (now - this.lastTickAt > this.frameInterval * this.maxCatchupFrames) {
        this.lastTickAt = now;
        this.simTime = now;
      }
      let frames = 0;
      while (now - this.lastTickAt >= this.frameInterval && frames < this.maxCatchupFrames) {
        this.simTime += this.frameInterval;
        this.renderTick(this.simTime);
        this.lastTickAt += this.frameInterval;
        frames++;
      }
    }
    this.animationId = requestAnimationFrame((t) => this.loop(t));
  }

  renderTick(now) {
    if (this.mic) this.mic.pumpLatestWindow();
    this.call('sim_tick', now);
    this.emitFrame(now);
  }

  renderFrame(now) {
    this.call('sim_render');
    this.emitFrame(now);
  }

  emitFrame(now) {
    const w = this.call('sim_width');
    const h = this.call('sim_height');
    const bytes = w * h * 3;
    const ptr = this.call('sim_framebuffer');
    if (!this.rgbFrame || this.rgbFrameBytes !== bytes) {
      this.rgbFrame = new Uint8Array(bytes);
      this.rgbFrameBytes = bytes;
    }
    this.frameNo++;
    this.rgbFrame.set(this.module.HEAPU8.subarray(ptr, ptr + bytes));
    this.callbacks.onRgbFrame(this.rgbFrame, w, h);
  }

  send(type, value) {
    if (type === 'button') {
      const payload = value || {};
      const sub = String(payload.value || '').toLowerCase();
      const count = Number.isFinite(Number(payload.count)) ? Number(payload.count) : 1;
      if (sub === 'press') this.call('sim_button_press', count);
      else if (sub === 'release') this.call('sim_button_release');
      else if (sub === 'tap') this.call('sim_button_tap', count);
      else if (sub === 'double') this.call('sim_button_tap', 2);
      else if (sub === 'triple') this.call('sim_button_tap', 3);
      else if (sub === 'hold') this.call('sim_button_hold');
      else if (sub === 'step') this.call('sim_button_step');
      return;
    }
    const v = Number(value);
    if (type === 'effect') this.call('sim_set_effect', v);
    else if (type === 'palette') this.call('sim_set_palette', v);
    else if (type === 'brightness') this.call('sim_set_brightness', v);
    else if (type === 'speed') this.call('sim_set_speed', v);
    else if (type === 'scale') this.call('sim_set_scale', v);
    else if (type === 'fps') { this.fps = FIXED_FPS; this.frameInterval = 1000 / FIXED_FPS; }
    else if (type === 'reset') { this.call('sim_reset_defaults'); this.callbacks.onSettings(this.currentEffectSettings()); }
    else if (type === 'pause') { this.paused = !!v; this.callbacks.onPaused(this.paused); }
    else if (type === 'step' && this.paused) {
      const now = performance.now();
      if (this.mic) this.mic.pumpLatestWindow();
      this.call('sim_update', now);
      this.renderFrame(now);
    }
    else if (type === 'power') {
      const mode = value === 'on' || value === '1' ? 1 : value === 'off' || value === '0' ? 0 : value === 'toggle' || value === '2' ? 2 : -1;
      if (mode >= 0) this.call('sim_power', mode);
    }
    else if (type === 'effectStep') {
      if (value === 'next') this.call('sim_effect_next');
      else if (value === 'prev') this.call('sim_effect_prev');
    }
    else if (type === 'notifyText') {
      const text = String(value || '').trim().slice(0, 160);
      if (!text || !this.module) return;
      this.module.ccall('sim_notify_text', 'number', ['string'], [text]);
    }
    else if (type === 'notifyUser') {
      const map = { notify: 1, warning: 3, alarm: 4 };
      const t = map[String(value || '').toLowerCase()];
      if (t) this.call('sim_notify_user', t);
    }
    else if (type === 'notifyClear') {
      this.call('sim_notify_clear');
    }
    else if (type === 'audioMicStart') {
      if (this.mic) return this.mic.start();
    }
    else if (type === 'audioMicStop') {
      if (this.mic) this.mic.stop();
    }
    else if (type === 'audioGain') {
      if (this.mic) return this.mic.setGain(v);
    }
    else if (type === 'audioMode') {
      const mode = Math.max(0, Math.min(4, Math.round(v)));
      saveState('audioMode', mode);
      this.call('sim_set_audio_mode', mode);
    }
    else if (type === 'audioBand') {
      const band = Math.max(0, Math.min(2, Math.round(v)));
      saveState('audioBand', band);
      this.call('sim_set_audio_band', band);
    }
    else if (type === 'audioAmount') {
      const amount = clampByte(v);
      saveState('audioAmount', amount);
      this.call('sim_set_audio_amount', amount);
    }
    else if (type === 'audioTelemetry') {
      return this.mic ? this.mic.telemetry() : null;
    }
  }

  currentEffectSettings() {
    const entry = findEffect(currentEffectId) || (this.config && this.config.effects[0]);
    return entry && entry.settings ? entry.settings : {};
  }

  close() {
    this.closed = true;
    if (this.animationId) cancelAnimationFrame(this.animationId);
    if (this.onVisibilityChange) document.removeEventListener('visibilitychange', this.onVisibilityChange);
    if (this.mic) this.mic.stop();
  }
}

class BrowserMicInput {
  constructor(runner) {
    this.runner = runner;
    this.status = 'stopped';
    this.gainPercent = clampMicGain(hasStored('micGain') ? storedState.micGain : 100);
    this.context = null;
    this.stream = null;
    this.source = null;
    this.analyser = null;
    this.waveform = null;
    this.fftSize = 64;
    this.running = false;
    this.lastError = '';
    this.startToken = 0;
  }

  async start() {
    if (this.running || this.stream || this.context) this.stop();
    const token = ++this.startToken;
    this.status = 'starting';
    this.lastError = '';
    if (!window.isSecureContext) {
      this.fail('Microphone needs HTTPS or localhost');
      return false;
    }
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
      this.fail('Microphone API unsupported');
      return false;
    }

    let context = null;
    let stream = null;
    let source = null;
    let analyser = null;
    try {
      this.fftSize = Math.max(1, this.runner.call('sim_audio_fft_size') || 64);
      const AudioContextClass = window.AudioContext || window.webkitAudioContext;
      if (!AudioContextClass) throw new Error('AudioContext unsupported');

      context = new AudioContextClass();
      if (context.state === 'suspended') await context.resume();
      try {
        stream = await navigator.mediaDevices.getUserMedia({
          audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false },
        });
      } catch {
        if (token !== this.startToken) {
          this.cleanupAudioResources(source, analyser, stream, context);
          return false;
        }
        stream = await navigator.mediaDevices.getUserMedia({ audio: true });
      }

      if (token !== this.startToken) {
        this.cleanupAudioResources(source, analyser, stream, context);
        return false;
      }

      source = context.createMediaStreamSource(stream);
      analyser = context.createAnalyser();
      analyser.fftSize = 2048;
      analyser.smoothingTimeConstant = 0;
      source.connect(analyser);

      if (token !== this.startToken) {
        this.cleanupAudioResources(source, analyser, stream, context);
        return false;
      }

      this.context = context;
      this.stream = stream;
      this.source = source;
      this.analyser = analyser;
      this.waveform = new Float32Array(analyser.fftSize);

      this.running = true;
      this.status = 'running';
      this.runner.call('sim_audio_flush');
      this.runner.call('sim_audio_set_enabled', 1);
      return true;
    } catch (err) {
      this.cleanupAudioResources(source, analyser, stream, context);
      if (token !== this.startToken) return false;
      this.fail(err && err.message ? err.message : 'Microphone start failed');
      return false;
    }
  }

  stop() {
    this.startToken++;
    this.running = false;
    this.status = 'stopped';
    this.runner.call('sim_audio_set_enabled', 0);
    this.runner.call('sim_audio_flush');

    this.cleanupNodes();
  }

  cleanupNodes() {
    this.cleanupAudioResources(this.source, this.analyser, this.stream, this.context);

    this.context = null;
    this.stream = null;
    this.source = null;
    this.analyser = null;
    this.waveform = null;
  }

  cleanupAudioResources(source, analyser, stream, context) {
    if (source) source.disconnect();
    if (analyser) analyser.disconnect();
    if (stream) {
      for (const track of stream.getTracks()) track.stop();
    }
    if (context && context.state !== 'closed') {
      context.close().catch(() => {});
    }
  }

  fail(message) {
    this.startToken++;
    this.status = 'error';
    this.lastError = message;
    this.running = false;
    this.runner.call('sim_audio_set_enabled', 0);
    this.runner.call('sim_audio_flush');
    this.cleanupNodes();
  }

  setGain(value) {
    this.gainPercent = clampMicGain(value);
    saveState('micGain', this.gainPercent);
    return this.gainPercent;
  }

  gain() {
    return this.gainPercent;
  }

  telemetry() {
    return {
      status: this.status,
      running: this.running,
      gain: this.gainPercent,
      error: this.lastError,
      level: this.runner.call('sim_audio_level'),
      bass: this.runner.call('sim_audio_bass'),
      treble: this.runner.call('sim_audio_treble'),
      available: !!this.runner.call('sim_audio_available'),
      underruns: this.runner.call('sim_audio_underruns'),
      overflows: this.runner.call('sim_audio_overflows'),
      mode: this.runner.call('sim_audio_mode'),
      band: this.runner.call('sim_audio_band'),
      amount: this.runner.call('sim_audio_amount'),
    };
  }

  pumpLatestWindow() {
    if (!this.running || !this.analyser || !this.waveform) return;
    try {
      this.analyser.getFloatTimeDomainData(this.waveform);
      this.runner.call('sim_audio_flush');
      const gain = (this.gainPercent / 100) * MIC_ADC_PREAMP;
      const step = this.waveform.length / this.fftSize;
      for (let i = 0; i < this.fftSize; i++) {
        const sample = this.waveform[Math.min(this.waveform.length - 1, Math.floor(i * step))] || 0;
        const adc = Math.max(0, Math.min(1023, Math.round(512 + sample * gain * 511)));
        this.runner.call('sim_audio_push_sample', adc);
      }
    } catch (err) {
      this.fail(err && err.message ? err.message : 'Microphone read failed');
    }
  }
}

function loadWasmGlue() {
  if (window.GyverLampSimModule) return Promise.resolve();
  return new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = 'wasm/gyverlamp_sim_wasm.js';
    script.onload = resolve;
    script.onerror = () => reject(new Error('Missing wasm/gyverlamp_sim_wasm.js'));
    document.head.appendChild(script);
  });
}

function connect() {
  activeRunner = new WasmRunner(callbacks());
}

function rerenderLastFrame() {
  if (!lastFrame) return;
  renderFrame(lastFrame.buf, lastFrame.offset, lastFrame.pixels, lastFrame.fw, lastFrame.fh);
}

function renderFrame(buf, offset, pixels, fw, fh) {
  if (preview.renderMode === 'cylinder') {
    renderCylinder(buf, offset, pixels, fw, fh);
  } else if (preview.renderMode === 'diffused') {
    renderDiffuser(buf, offset, fw, fh);
  } else {
    renderSharp(buf, offset, pixels, fw, fh);
  }
}

function ensureSharpBuffer(fw, fh) {
  if (canvas.width !== fw || canvas.height !== fh) {
    canvas.width = fw;
    canvas.height = fh;
  }
  if (!imageData || imageData.width !== fw || imageData.height !== fh) {
    imageData = ctx.createImageData(fw, fh);
  }
}

function renderSharp(buf, offset, pixels, fw, fh) {
  ensureSharpBuffer(fw, fh);
  for (let i = 0; i < pixels; i++) {
    imageData.data[i * 4 + 0] = applyBrightness(buf[offset + i * 3 + 0]);
    imageData.data[i * 4 + 1] = applyBrightness(buf[offset + i * 3 + 1]);
    imageData.data[i * 4 + 2] = applyBrightness(buf[offset + i * 3 + 2]);
    imageData.data[i * 4 + 3] = 255;
  }
  ctx.filter = 'none';
  canvas.style.filter = 'none';
  ctx.putImageData(imageData, 0, 0);
}

function renderDiffuser(buf, offset, fw, fh) {
  paintDiffuserInto({
    destCtx: ctx,
    destCanvas: canvas,
    srcCtx: diffuserSourceCtx,
    srcCanvas: diffuserSource,
    bloomCtx: diffuserBloomSourceCtx,
    bloomCanvas: diffuserBloomSource,
    buf, offset, fw, fh,
    cssBlurCanvas: canvas,
  });
}

// Shared diffuser pipeline. Paints LEDs onto srcCanvas, bloom source onto bloomCanvas,
// then composites plastic substrate + blurred LEDs + bloom onto destCanvas.
// Used by both renderDiffuser (visible 2D canvas) and renderCylinder (offscreen texture).
// opts.cssBlurCanvas: DOM canvas to apply CSS blur fallback on (Safari, 2D mode only).
// opts.forceCanvasFilter: always use ctx.filter for blur even on browsers where it is
//   normally disabled (needed for cylinder, since a WebGL texture cannot use CSS blur).
function paintDiffuserInto(o) {
  const cell = o.cellSize || diffuserCellSize;
  const cw = o.fw * cell;
  const ch = o.fh * cell;
  for (const c of [o.destCanvas, o.srcCanvas, o.bloomCanvas]) {
    if (c.width !== cw || c.height !== ch) {
      c.width = cw;
      c.height = ch;
    }
  }

  const gapPx = Math.min(cell - 1, Math.max(0, cell * FIXED_LED_GAP / 100));
  const ledSize = Math.max(1, cell - gapPx);
  const pad = (cell - ledSize) / 2;

  o.srcCtx.filter = 'none';
  o.srcCtx.clearRect(0, 0, o.srcCanvas.width, o.srcCanvas.height);
  o.bloomCtx.filter = 'none';
  o.bloomCtx.clearRect(0, 0, o.bloomCanvas.width, o.bloomCanvas.height);

  for (let y = 0; y < o.fh; y++) {
    for (let x = 0; x < o.fw; x++) {
      const i = y * o.fw + x;
      const r = applyBrightness(o.buf[o.offset + i * 3 + 0]);
      const g = applyBrightness(o.buf[o.offset + i * 3 + 1]);
      const b = applyBrightness(o.buf[o.offset + i * 3 + 2]);
      const px = x * cell + pad;
      const py = y * cell + pad;
      o.srcCtx.fillStyle = `rgb(${r}, ${g}, ${b})`;
      o.srcCtx.fillRect(px, py, ledSize, ledSize);

      const luminance = Math.max(r, g, b);
      if (luminance > 32) {
        const bloomGain = Math.min(1, (luminance - 32) / 223);
        o.bloomCtx.fillStyle = `rgba(${r}, ${g}, ${b}, ${bloomGain})`;
        o.bloomCtx.fillRect(px, py, ledSize, ledSize);
      }
    }
  }

  const blur = Math.max(0, preview.blur);
  const canCanvasFilter = useCanvasFilter || o.forceCanvasFilter;
  const canBlur = canCanvasFilter && blur > 0;

  const dest = o.destCtx;
  dest.fillStyle = diffuserBackgroundColor();
  dest.fillRect(0, 0, o.destCanvas.width, o.destCanvas.height);
  dest.filter = canBlur ? `blur(${blur}px)` : 'none';
  if (o.cssBlurCanvas) {
    o.cssBlurCanvas.style.filter = !useCanvasFilter && blur > 0 ? `blur(${blur}px)` : 'none';
  }
  dest.globalCompositeOperation = 'source-over';
  dest.globalAlpha = 0.88;
  dest.drawImage(o.srcCanvas, 0, 0);

  const bloom = clampPercent(preview.bloom);
  if (bloom > 0) {
    dest.globalCompositeOperation = 'lighter';
    dest.globalAlpha = bloom / 100 * 0.7;
    dest.filter = canBlur ? `blur(${Math.max(1, blur * 2.4)}px)` : 'none';
    dest.drawImage(o.bloomCanvas, 0, 0);
  }

  const veil = FIXED_MILKINESS;
  if (veil > 0) {
    dest.globalCompositeOperation = 'source-over';
    dest.filter = 'none';
    dest.globalAlpha = veil / 100 * 0.45;
    dest.fillStyle = 'rgb(255, 255, 248)';
    dest.fillRect(0, 0, o.destCanvas.width, o.destCanvas.height);
  }

  dest.filter = 'none';
  dest.globalAlpha = 1;
  dest.globalCompositeOperation = 'source-over';
}

function paintCylinderLedTextureInto(o) {
  const cell = o.cellSize || diffuserCellSize;
  const cw = o.fw * cell;
  const ch = o.fh * cell;
  if (o.canvas.width !== cw || o.canvas.height !== ch) {
    o.canvas.width = cw;
    o.canvas.height = ch;
  }

  const gapPx = Math.min(cell - 1, Math.max(0, cell * FIXED_LED_GAP / 100));
  const ledSize = Math.max(1, cell - gapPx);
  const pad = (cell - ledSize) / 2;
  const ctx2d = o.ctx;

  ctx2d.filter = 'none';
  ctx2d.globalAlpha = 1;
  ctx2d.globalCompositeOperation = 'source-over';
  ctx2d.fillStyle = 'rgb(0, 0, 0)';
  ctx2d.fillRect(0, 0, cw, ch);

  for (let y = 0; y < o.fh; y++) {
    for (let x = 0; x < o.fw; x++) {
      const i = y * o.fw + x;
      const r = applyBrightness(o.buf[o.offset + i * 3 + 0]);
      const g = applyBrightness(o.buf[o.offset + i * 3 + 1]);
      const b = applyBrightness(o.buf[o.offset + i * 3 + 2]);
      const px = x * cell + pad;
      const py = y * cell + pad;

      ctx2d.fillStyle = `rgb(${r}, ${g}, ${b})`;
      ctx2d.fillRect(px, py, ledSize, ledSize);
    }
  }
}

function paintCylinderShadeTextureInto(o) {
  const cell = o.cellSize || diffuserCellSize;
  const cw = o.fw * cell;
  const ch = o.fh * cell;
  if (o.canvas.width !== cw || o.canvas.height !== ch) {
    o.canvas.width = cw;
    o.canvas.height = ch;
  }
  if (o.workCanvas.width !== cw * 3 || o.workCanvas.height !== ch) {
    o.workCanvas.width = cw * 3;
    o.workCanvas.height = ch;
  }

  const blur = Math.max(1, preview.blur);
  const veil = FIXED_MILKINESS;
  const background = FIXED_PLASTIC;

  const work = o.workCtx;
  work.filter = 'none';
  work.globalAlpha = 1;
  work.globalCompositeOperation = 'source-over';
  work.clearRect(0, 0, o.workCanvas.width, o.workCanvas.height);

  // Repeat horizontally before blur so light wraps cleanly across cylinder seam.
  for (const x of [0, cw, cw * 2]) work.drawImage(o.sourceCanvas, x, 0);

  const dest = o.ctx;
  dest.filter = 'none';
  dest.globalAlpha = 1;
  dest.globalCompositeOperation = 'source-over';
  dest.clearRect(0, 0, cw, ch);

  // Opaque white diffuser base. Colored light appears via blurred emissive map,
  // not by making the shade itself transparent.
  const milkiness = veil / 100;
  const plastic = background / 100;
  const baseAlpha = Math.min(1, 0.72 + milkiness * 0.22 + plastic * 0.08);
  dest.fillStyle = `rgba(255, 255, 255, ${baseAlpha.toFixed(3)})`;
  dest.fillRect(0, 0, cw, ch);

  // Main diffusion lobe. Canvas blur is isotropic, but wrapping X makes the
  // cylinder read as plastic scattering instead of flat screen blur.
  dest.globalCompositeOperation = 'source-over';
  dest.globalAlpha = 0.5 - milkiness * 0.28;
  dest.filter = `blur(${Math.max(1, blur * 1.35)}px)`;
  dest.drawImage(o.workCanvas, cw, 0, cw, ch, 0, 0, cw, ch);

  // Wider low-energy diffusion lobe. It is intentionally not tied to Bloom:
  // Bloom is a postprocess pass, not baked light leaking at texture borders.
  if (veil > 0) {
    dest.globalAlpha = 0.12 + (1 - milkiness) * 0.08;
    dest.filter = `blur(${Math.max(2, blur * 3.2)}px)`;
    dest.drawImage(o.workCanvas, cw, 0, cw, ch, 0, 0, cw, ch);
  }

  // Fixed milky veil. Keep it out of UI because white emissive veil also feeds bloom.
  if (milkiness > 0) {
    dest.filter = 'none';
    dest.globalAlpha = milkiness * 0.32;
    dest.fillStyle = '#ffffff';
    dest.fillRect(0, 0, cw, ch);
  }

  dest.filter = 'none';
  dest.globalAlpha = 1;
  dest.globalCompositeOperation = 'source-over';
}

function renderCylinderScene() {
  if (!cylinder.renderer || !cylinder.scene || !cylinder.camera) return;
  if (cylinder.composer && cylinder.postReady) {
    cylinder.composer.render();
  } else {
    cylinder.renderer.render(cylinder.scene, cylinder.camera);
  }
}

function updateCylinderPhysicalParams() {
  if (!cylinder.shadeMesh) return;

  const bloom = clampPercent(preview.bloom);
  const bloomAmount = bloom / 100;
  const blur = clampPercent(Math.round(preview.blur * 100 / diffuserMaxBlurPx));
  const veil = FIXED_MILKINESS;

  const shade = cylinder.shadeMesh.material;
  shade.opacity = 1;
  shade.roughness = 0.78 + blur / 100 * 0.2;
  shade.transmission = 0;
  shade.thickness = 1.2 + blur / 100 * 3.2;
  shade.attenuationDistance = 0.55 + (100 - veil) / 100 * 1.35;
  shade.emissiveIntensity = 0.22 + bloomAmount * 0.55 + (1 - veil / 100) * 0.14;
  shade.needsUpdate = true;

  if (cylinder.bloomPass) {
    cylinder.bloomPass.strength = Math.pow(bloomAmount, 1.35) * 0.68;
    cylinder.bloomPass.radius = 0.08 + blur / 100 * 0.32;
    cylinder.bloomPass.threshold = 0.46;
  }
}

function initCylinderPostProcessing() {
  if (cylinder.postReady || cylinder.postLoading || !cylinder.renderer || !cylinder.scene || !cylinder.camera) return;
  cylinder.postLoading = true;

  Promise.all([
    import('three/addons/postprocessing/EffectComposer.js'),
    import('three/addons/postprocessing/RenderPass.js'),
    import('three/addons/postprocessing/UnrealBloomPass.js'),
    import('three/addons/postprocessing/OutputPass.js'),
  ]).then(([composerMod, renderPassMod, bloomPassMod, outputPassMod]) => {
    const EffectComposer = composerMod.EffectComposer;
    const RenderPass = renderPassMod.RenderPass;
    const UnrealBloomPass = bloomPassMod.UnrealBloomPass;
    const OutputPass = outputPassMod.OutputPass;

    cylinder.composer = new EffectComposer(cylinder.renderer);
    cylinder.composer.addPass(new RenderPass(cylinder.scene, cylinder.camera));
    cylinder.bloomPass = new UnrealBloomPass(new THREE.Vector2(1, 1), 1.1, 0.45, 0.15);
    cylinder.composer.addPass(cylinder.bloomPass);
    cylinder.composer.addPass(new OutputPass());
    cylinder.postReady = true;
    cylinder.postLoading = false;
    updateCylinderPhysicalParams();
    resizeCylinder();
    renderCylinderScene();
  }).catch((err) => {
    console.error('Cylinder bloom postprocessing failed to load', err);
    cylinder.postLoading = false;
  });
}

function ensureCylinder() {
  if (cylinder.initialized) return;
  if (typeof THREE === 'undefined') {
    console.error('Three.js failed to load (CDN). Cylinder mode unavailable.');
    stageInfo.textContent = 'Cylinder: Three.js CDN unavailable';
    return;
  }

  // preserveDrawingBuffer is required because render() is driven by the WASM
  // RGB-frame callback (WebSocket message), not requestAnimationFrame. Without
  // it the browser clears the framebuffer before the compositor samples it and
  // the visible canvas stays black.
  cylinder.renderer = new THREE.WebGLRenderer({
    canvas: cylinderCanvasEl,
    antialias: false,
    preserveDrawingBuffer: true,
  });
  cylinder.renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
  cylinder.renderer.setClearColor(0x000000, 1);
  cylinder.renderer.outputColorSpace = THREE.SRGBColorSpace;
  cylinder.renderer.toneMapping = THREE.ACESFilmicToneMapping;
  cylinder.renderer.toneMappingExposure = 1.1;

  cylinder.scene = new THREE.Scene();
  cylinder.scene.background = new THREE.Color(0x000000);
  cylinder.scene.add(new THREE.AmbientLight(0xffffff, 0.35));
  const keyLight = new THREE.PointLight(0xffffff, 1.1, 100);
  keyLight.position.set(0, 0, 8);
  cylinder.scene.add(keyLight);

  buildCylinderGeometry();

  cylinder.camera = new THREE.PerspectiveCamera(50, 1, 0.1, 1000);
  fitCylinderCamera();
  initCylinderPostProcessing();

  cylinder.resizeObs = new ResizeObserver(() => resizeCylinder());
  cylinder.resizeObs.observe(cylinderCanvasEl);
  window.addEventListener('resize', resizeCylinder);

  bindCylinderPointer();

  cylinder.initialized = true;
}

function buildCylinderGeometry() {
  // Radial segments: bump well above the LED column count for a smooth
  // cylindrical surface (16 columns alone gives a visibly polygonal tube).
  const segW = Math.max(64, Math.round(width) * 4);
  const segH = Math.max(1, Math.round(height));
  cylinder.geoW = width;
  cylinder.geoH = height;

  // The cylinder represents the shade (diffuser), not the bare LED matrix.
  // Matrix radius (in lamp units) is width/(2*pi); the shade is ~2.1x wider.
  const matrixRadius = Math.max(0.25, width / (2 * Math.PI));
  const radius = Math.max(0.5, matrixRadius * SHADE_RATIO);
  const h = Math.max(1, height);
  const geo = new THREE.CylinderGeometry(radius, radius, h, segW, segH, true);

  // Offscreen canvas for raw emissive LED texture. Size is POT to keep the
  // Chromium/Metal CanvasTexture upload path stable.
  // IMPORTANT: pre-size the texture canvas to its final POT dimensions BEFORE
  // the CanvasTexture is created. The default canvas is 300x150 (NPOT); if
  // Three.js's first upload happens with NPOT dimensions it leaves the GPU
  // texture object in a permanently broken state on Chromium/Metal, and even
  // later POT re-uploads render solid black.
  const cylCellSize = (() => {
    const natural = width * diffuserCellSize;
    const target = Math.pow(2, Math.ceil(Math.log2(natural)));
    return Math.max(diffuserCellSize, Math.floor(target / width));
  })();
  const texW = width * cylCellSize;
  const texH = height * cylCellSize;
  if (!cylinder.canvas) {
    cylinder.canvas = document.createElement('canvas');
    cylinder.canvas.width = texW;
    cylinder.canvas.height = texH;
    cylinder.ctx = cylinder.canvas.getContext('2d');
    cylinder.shadeCanvas = document.createElement('canvas');
    cylinder.shadeCanvas.width = texW;
    cylinder.shadeCanvas.height = texH;
    cylinder.shadeCtx = cylinder.shadeCanvas.getContext('2d');
    cylinder.shadeWorkCanvas = document.createElement('canvas');
    cylinder.shadeWorkCanvas.width = texW * 3;
    cylinder.shadeWorkCanvas.height = texH;
    cylinder.shadeWorkCtx = cylinder.shadeWorkCanvas.getContext('2d');
  }

  if (!cylinder.texture) {
    cylinder.texture = new THREE.CanvasTexture(cylinder.canvas);
    cylinder.texture.magFilter = THREE.LinearFilter;
    // NOTE: mipmaps disabled — keeps the GPU upload path simple and avoids
    // generateMipmap edge cases. The texture is already high-res enough.
    cylinder.texture.minFilter = THREE.LinearFilter;
    cylinder.texture.generateMipmaps = false;
    cylinder.texture.colorSpace = THREE.SRGBColorSpace;
  } else {
    cylinder.texture.image = cylinder.canvas;
    cylinder.texture.needsUpdate = true;
  }

  if (!cylinder.shadeTexture) {
    cylinder.shadeTexture = new THREE.CanvasTexture(cylinder.shadeCanvas);
    cylinder.shadeTexture.magFilter = THREE.LinearFilter;
    cylinder.shadeTexture.minFilter = THREE.LinearFilter;
    cylinder.shadeTexture.generateMipmaps = false;
    cylinder.shadeTexture.colorSpace = THREE.SRGBColorSpace;
  } else {
    cylinder.shadeTexture.image = cylinder.shadeCanvas;
    cylinder.shadeTexture.needsUpdate = true;
  }

  if (!cylinder.shadeMesh) {
    const shadeMat = new THREE.MeshPhysicalMaterial({
      color: 0xffffff,
      map: cylinder.shadeTexture,
      emissive: 0xffffff,
      emissiveMap: cylinder.shadeTexture,
      emissiveIntensity: 0.35,
      side: THREE.DoubleSide,
      transparent: false,
      opacity: 1,
      depthWrite: true,
      roughness: 0.82,
      metalness: 0,
      transmission: 0,
      thickness: 2.2,
      attenuationColor: 0xffffff,
      attenuationDistance: 1.1,
      ior: 1.45,
    });
    cylinder.shadeMesh = new THREE.Mesh(geo, shadeMat);
    cylinder.scene.add(cylinder.shadeMesh);
  } else {
    cylinder.shadeMesh.geometry.dispose();
    cylinder.shadeMesh.geometry = geo;
    cylinder.shadeMesh.material.map = cylinder.shadeTexture;
    cylinder.shadeMesh.material.emissiveMap = cylinder.shadeTexture;
  }
  cylinder.shadeMesh.rotation.y = cylinder.yaw;
  updateCylinderPhysicalParams();
}

// Fit camera so the whole shade cylinder is visible. Vertical fit uses the
// perspective FOV directly; horizontal fit accounts for the current aspect
// ratio (recomputed on resize). The larger of the two wins, plus a small margin.
function fitCylinderCamera() {
  if (!cylinder.camera) return;
  const fovDeg = 50;
  const halfFov = (fovDeg * Math.PI / 180) / 2;
  const matrixRadius = Math.max(0.25, width / (2 * Math.PI));
  const shadeRadius = Math.max(0.5, matrixRadius * SHADE_RATIO);
  const shadeH = Math.max(1, height);
  const shadeW = shadeRadius * 2;

  const distH = (shadeH / 2) / Math.tan(halfFov);
  const aspect = cylinder.camera.aspect || 1;
  const halfHFov = Math.atan(Math.tan(halfFov) * aspect);
  const distW = halfHFov > 0 ? (shadeW / 2) / Math.tan(halfHFov) : distH;
  // Fit with a margin so the whole shade sits comfortably inside the canvas
  // (1.25 = ~20% padding). userZoom multiplies on top — wheel-driven zoom
  // persists across resize and mode-switch by reading this scalar.
  const dist = Math.max(distH, distW) * 1.25 * (cylinder.userZoom || 1);

  cylinder.camera.fov = fovDeg;
  cylinder.camera.position.set(0, 0, dist);
  cylinder.camera.lookAt(0, 0, 0);
  cylinder.camera.updateProjectionMatrix();
}

function bindCylinderPointer() {
  const el = cylinderCanvasEl;

  const pointerX = (e) => {
    if (e.touches && e.touches.length) return e.touches[0].clientX;
    return e.clientX;
  };

  const onDown = (e) => {
    cylinder.dragging = true;
    cylinder.lastX = pointerX(e);
    el.style.cursor = 'grabbing';
    if (e.cancelable) e.preventDefault();
  };
  const onMove = (e) => {
    if (!cylinder.dragging) return;
    const x = pointerX(e);
    const dx = x - cylinder.lastX;
    cylinder.lastX = x;
    cylinder.yaw -= dx * 0.01;
    if (cylinder.shadeMesh) cylinder.shadeMesh.rotation.y = cylinder.yaw;
    renderCylinderScene();
    if (e.cancelable) e.preventDefault();
  };
  const onUp = () => {
    if (!cylinder.dragging) return;
    cylinder.dragging = false;
    el.style.cursor = 'grab';
  };

  el.addEventListener('mousedown', onDown);
  window.addEventListener('mousemove', onMove);
  window.addEventListener('mouseup', onUp);
  el.addEventListener('touchstart', onDown, { passive: false });
  window.addEventListener('touchmove', onMove, { passive: false });
  window.addEventListener('touchend', onUp);
  window.addEventListener('touchcancel', onUp);

  // Wheel zoom — adjust userZoom (multiplier on top of fitCylinderCamera's
  // computed distance) and refit. Clamped so the cylinder can't disappear
  // or clip into the camera. preventDefault stops page scroll over the canvas.
  const onWheel = (e) => {
    if (!cylinder.camera) return;
    if (e.cancelable) e.preventDefault();
    const factor = e.deltaY < 0 ? 0.9 : 1.1;
    cylinder.userZoom = Math.min(3.0, Math.max(0.4, cylinder.userZoom * factor));
    fitCylinderCamera();
  };
  el.addEventListener('wheel', onWheel, { passive: false });
}

function resizeCylinder() {
  if (!cylinder.initialized || !cylinder.renderer) return;
  const w = cylinderCanvasEl.clientWidth;
  const h = cylinderCanvasEl.clientHeight;
  if (!w || !h) return;
  cylinder.renderer.setSize(w, h, false);
  if (cylinder.composer) cylinder.composer.setSize(w, h);
  cylinder.camera.aspect = w / h;
  fitCylinderCamera();
  renderCylinderScene();
}

function renderCylinder(buf, offset, pixels, fw, fh) {
  ensureCylinder();
  if (!cylinder.initialized) return;

  if (cylinder.geoW !== width || cylinder.geoH !== height) {
    buildCylinderGeometry();
  }

  // Paint raw LEDs onto the inner emissive cylinder and a separate blurred,
  // horizontally wrapped texture onto the outer shade so the plastic itself
  // glows/diffuses instead of acting like a transparent shell.
  // Use a power-of-two cellSize so the resulting canvas is POT — Three.js r160
  // + Chrome/Metal on Mac fails with GL_INVALID_VALUE on NPOT canvas textures.
  const cylCellSize = (() => {
    const natural = fw * diffuserCellSize;
    const target = Math.pow(2, Math.ceil(Math.log2(natural)));
    return Math.max(diffuserCellSize, Math.floor(target / fw));
  })();
  paintCylinderLedTextureInto({
    ctx: cylinder.ctx,
    canvas: cylinder.canvas,
    buf, offset, fw, fh,
    cellSize: cylCellSize,
  });
  paintCylinderShadeTextureInto({
    ctx: cylinder.shadeCtx,
    canvas: cylinder.shadeCanvas,
    workCtx: cylinder.shadeWorkCtx,
    workCanvas: cylinder.shadeWorkCanvas,
    sourceCanvas: cylinder.canvas,
    fw, fh,
    cellSize: cylCellSize,
  });

  cylinder.texture.needsUpdate = true;
  cylinder.shadeTexture.needsUpdate = true;
  if (cylinder.shadeMesh) cylinder.shadeMesh.rotation.y = cylinder.yaw;
  updateCylinderPhysicalParams();
  renderCylinderScene();
}

setInterval(() => {
  if (lastFrameAt) {
    const age = Date.now() - lastFrameAt;
    lastFrameAge.textContent = age < 1000 ? `${age} ms` : `${(age / 1000).toFixed(1)} s`;
  } else {
    lastFrameAge.textContent = '—';
  }
  frameInfo.textContent = currentFps !== null ? `${currentFps} fps` : '—';
  refreshAudioUi();
}, 250);

connect();
refreshAudioUi();
initVersionBadges();

function setVersionBadge(badge, text) {
  if (!badge) return;
  const valueEl = badge.querySelector('.value');
  if (valueEl) valueEl.textContent = text;
}

async function readVersionResponse(result) {
  if (result.status !== 'fulfilled' || !result.value || !result.value.ok) return '?';
  try {
    const text = (await result.value.text()).trim();
    return text || '?';
  } catch {
    return '?';
  }
}

async function initVersionBadges() {
  try {
    const results = await Promise.allSettled([
      fetch('firmware-version.txt'),
      fetch('sim-version.txt'),
    ]);
    const fwText = await readVersionResponse(results[0]);
    const simText = await readVersionResponse(results[1]);
    setVersionBadge(firmwareVersionBadge, fwText);
    setVersionBadge(simVersionBadge, simText);
  } catch {
    setVersionBadge(firmwareVersionBadge, '?');
    setVersionBadge(simVersionBadge, '?');
  }
}
