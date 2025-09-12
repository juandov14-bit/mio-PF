async function fetchJSON(path, opts) {
  const res = await fetch(path, opts);
  const ct = (res.headers.get('content-type') || '').toLowerCase();
  let body;
  try {
    if (ct.includes('application/json')) body = await res.json();
    else body = await res.text();
  } catch (e) {
    // Evita fallar por 204 o cuerpos vacíos; devuelve texto crudo
    body = '';
  }
  if (!res.ok) throw new Error(typeof body === 'string' && body ? body : `${res.status} ${res.statusText}`);
  // Si vino texto, devuelve un objeto informativo
  if (typeof body === 'string') return { ok: true, raw: body };
  return body;
}

function setPre(id, data) {
  document.getElementById(id).textContent = JSON.stringify(data, null, 2);
}

// Status lamp polling
async function pollHealth() {
  const lamp = document.getElementById('lamp');
  try {
    const data = await fetchJSON('/api/health');
    if (data && (data.status === 'ok' || data.ok)) {
      lamp.classList.add('on');
    } else {
      lamp.classList.remove('on');
    }
  } catch {
    lamp.classList.remove('on');
  }
}
setInterval(pollHealth, 3000);
pollHealth();

document.getElementById('btnSensors').addEventListener('click', async () => {
  try { setPre('sensors', await fetchJSON('/api/sensors')); }
  catch (e) { setPre('sensors', { error: String(e) }); }
});

// Show effective backend (filled by nginx proxy to /api/*)
document.getElementById('backendUrl').textContent = 'API: /api/* (IP del ESP32)';

// --- Config form logic ---
const modeEl = document.getElementById('mode');
const nodeEl = document.getElementById('node');
const targetTempEl = document.getElementById('targetTemp');
const coolerPercentEl = document.getElementById('coolerPercent');
const pwmPercentEl = document.getElementById('pwmPercent');

function updateModeVisibility() {
  const mode = modeEl.value;
  const showPid = (mode === 'pid');
  document.querySelectorAll('.pid-only').forEach(el => el.classList.toggle('hidden', !showPid));
  document.querySelectorAll('.fixed-only').forEach(el => el.classList.toggle('hidden', mode !== 'fixed'));
}

modeEl.addEventListener('change', updateModeVisibility);
updateModeVisibility();

document.getElementById('applyConfig').addEventListener('click', async () => {
  const mode = modeEl.value;
  const cooler = Number(coolerPercentEl.value);
  const actions = [];
  try {
    actions.push(fetchJSON(`/api/cooler?percent=${cooler}`, { method: 'POST' }));
    if (mode === 'pid') {
      const node = Number(nodeEl.value);
      const targetTemp = Number(targetTempEl.value);
      actions.push(fetchJSON(`/api/mode?type=pid`, { method: 'POST' }));
      actions.push(fetchJSON(`/api/node?index=${node}`, { method: 'POST' }));
      actions.push(fetchJSON(`/api/setpoint?temp=${targetTemp.toFixed(1)}`, { method: 'POST' }));
    } else {
      const pwmPercent = Number(pwmPercentEl.value);
      actions.push(fetchJSON(`/api/mode?type=fixed`, { method: 'POST' }));
      actions.push(fetchJSON(`/api/fixed?percent=${pwmPercent}`, { method: 'POST' }));
    }
    const results = await Promise.all(actions);
    setPre('configResult', { ok: true, results });
  } catch (e) {
    setPre('configResult', { error: String(e) });
  }
});

document.getElementById('btnRun').addEventListener('click', async () => {
  try { setPre('configResult', await fetchJSON('/api/run', { method: 'POST' })); }
  catch (e) { setPre('configResult', { error: String(e) }); }
});
document.getElementById('btnStop').addEventListener('click', async () => {
  try { setPre('configResult', await fetchJSON('/api/stop', { method: 'POST' })); }
  catch (e) { setPre('configResult', { error: String(e) }); }
});

// --- Simple realtime chart ---
const chartCanvas = document.getElementById('chart');
const ctx = chartCanvas.getContext('2d');
// Series: 4 sensores + Ambiente + Acción (%)
const seriesDefs = [
  { key: 'S1', color: '#60A5FA' },
  { key: 'S2', color: '#34D399' },
  { key: 'S3', color: '#FBBF24' },
  { key: 'S4', color: '#F472B6' },
  { key: 'Ambiente', color: '#A78BFA' },
  { key: 'Accion %', color: '#EF4444' },
];
document.getElementById('legend').innerHTML = seriesDefs.map(s => `<span class="key"><span class="swatch" style="background:${s.color}"></span>${s.key}</span>`).join(' ');

const history = []; // { t, temps:[4], ambient:number, control_pct:number }
const MAX_POINTS = 240; // ~4 minutos a 1 Hz

// Resize canvas to fill its container (crisp with DPR)
function resizeCanvas() {
  const dpr = window.devicePixelRatio || 1;
  const displayWidth = Math.max(1, Math.floor(chartCanvas.clientWidth * dpr));
  const displayHeight = Math.max(1, Math.floor(chartCanvas.clientHeight * dpr));
  if (chartCanvas.width !== displayWidth || chartCanvas.height !== displayHeight) {
    chartCanvas.width = displayWidth;
    chartCanvas.height = displayHeight;
  }
}

function drawChart() {
  resizeCanvas();
  const W = chartCanvas.width; const H = chartCanvas.height;
  ctx.clearRect(0,0,W,H);
  ctx.lineCap = 'round';
  ctx.lineJoin = 'round';
  // Axes box
  const L = 60, R = 60, T = 14, B = 46;
  ctx.strokeStyle = '#22314b'; ctx.lineWidth = 1.5;
  ctx.beginPath(); ctx.moveTo(L, T); ctx.lineTo(L, H-B); ctx.lineTo(W-R, H-B); ctx.stroke();

  if (history.length < 2) return;
  // Rango fijo para facilitar lectura de pendientes
  const FIXED_MIN_T = 15;
  const FIXED_MAX_T = 60;
  const minY = FIXED_MIN_T;
  const maxY = FIXED_MAX_T;

  const t0 = history[0].t, t1 = history[history.length-1].t;
  const dt = (t1 - t0) || 1;
  const x = (t) => L + (W-L-R) * ((t - t0) / dt);
  const yT = (v) => T + (H-T-B) * (1 - (v - minY) / (maxY - minY || 1));
  const yU = (pct) => T + (H-T-B) * (1 - Math.min(pct/100, 1)); // 0..100%

  // Gridlines (temperatura, 5 divisiones)
  ctx.strokeStyle = '#1a2539'; ctx.lineWidth = 1; ctx.setLineDash([3, 6]);
  for (let i = 1; i <= 4; i++) {
    const frac = i / 5; // 0.2..0.8
    const y = T + (H-T-B) * (1 - frac);
    ctx.beginPath(); ctx.moveTo(L, y); ctx.lineTo(W-R, y); ctx.stroke();
  }
  ctx.setLineDash([]);

  // Grid labels
  ctx.fillStyle = '#a9b6c6'; ctx.font = '16px sans-serif';
  ctx.fillText(`${minY.toFixed(0)}°C`, 8, yT(minY));
  ctx.fillText(`${maxY.toFixed(0)}°C`, 8, yT(maxY));
  // Axis labels
  ctx.save();
  ctx.translate(18, (H - B + T) / 2);
  ctx.rotate(-Math.PI / 2);
  ctx.fillText('Temperatura (°C)', 0, 0);
  ctx.restore();
  // Eje derecho para acción de control
  ctx.textAlign = 'right';
  ctx.fillText(`0%`, W-8, yU(0));
  ctx.fillText(`100%`, W-8, yU(100));
  ctx.fillText('Acción (%)', W-8, T + 14);
  ctx.textAlign = 'left';

  // Plot temperatures
  for (let si=0; si<4; si++) {
    ctx.strokeStyle = seriesDefs[si].color; ctx.lineWidth = 2.5; ctx.beginPath();
    history.forEach((h, i) => {
      const v = (h.temps||[])[si]; if (typeof v !== 'number') return;
      const X = x(h.t); const Y = yT(v);
      if (i===0) ctx.moveTo(X,Y); else ctx.lineTo(X,Y);
    });
    ctx.stroke();
  }

  // Ambiente
  ctx.strokeStyle = seriesDefs[4].color; ctx.lineWidth = 2.5; ctx.setLineDash([6,4]); ctx.beginPath();
  history.forEach((h, i) => {
    const v = h.ambient; if (typeof v !== 'number') return;
    const X = x(h.t); const Y = yT(v);
    if (i===0) ctx.moveTo(X,Y); else ctx.lineTo(X,Y);
  });
  ctx.stroke();
  ctx.setLineDash([]);

  // Acción de control (porcentaje, eje derecho)
  ctx.strokeStyle = seriesDefs[5].color; ctx.lineWidth = 2.5; ctx.beginPath();
  history.forEach((h, i) => {
    const v = h.control_pct; if (typeof v !== 'number') return;
    const X = x(h.t); const Y = yU(v);
    if (i===0) ctx.moveTo(X,Y); else ctx.lineTo(X,Y);
  });
  ctx.stroke();
}

async function pollSensorsOnce() {
  try {
    const s = await fetchJSON('/api/state');
    const temps = s.temperatures ? s.temperatures.nodes : [];
    const ambient = s.temperatures ? s.temperatures.room : undefined;
    const control_pct = typeof s.control_pct === 'number' ? s.control_pct : (s.mode === 'pid' ? undefined : s.fixed_percent);
    // Actualiza panel de sensores (texto) además del gráfico
    setPre('sensors', {
      running: s.running,
      mode: s.mode,
      node: s.node,
      setpoint: s.setpoint,
      cooler_percent: s.cooler_percent,
      fixed_percent: s.fixed_percent,
      room: ambient,
      nodes: temps,
      control_pct
    });
    history.push({ t: Date.now(), temps, ambient, control_pct });
    if (history.length > MAX_POINTS) history.shift();
    drawChart();
  } catch (e) {
    console.error('Error al consultar /api/state:', e);
  }
}

let timer = null;
function updateTimer() {
  const enabled = document.getElementById('autorefresh').checked;
  const period = Math.max(200, Number(document.getElementById('period').value)||1000);
  if (timer) clearInterval(timer);
  if (enabled) {
    timer = setInterval(pollSensorsOnce, period);
  }
}
document.getElementById('autorefresh').addEventListener('change', updateTimer);
document.getElementById('period').addEventListener('change', updateTimer);
updateTimer();
pollSensorsOnce();

// Redimensionar al cambiar tamaño de ventana
window.addEventListener('resize', () => { resizeCanvas(); drawChart(); });
// Tamaño inicial del canvas
resizeCanvas();
