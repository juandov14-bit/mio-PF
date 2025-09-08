async function fetchJSON(path) {
  const res = await fetch(path);
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
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
document.getElementById('backendUrl').textContent = 'API: proxied by Nginx at /api/*';

// --- Config form logic ---
const modeEl = document.getElementById('mode');
const nodeEl = document.getElementById('node');
const targetTempEl = document.getElementById('targetTemp');
const coolerSpeedEl = document.getElementById('coolerSpeed');
const pwmPercentEl = document.getElementById('pwmPercent');

function updateModeVisibility() {
  const mode = modeEl.value;
  const showPidLike = (mode === 'pid' || mode === 'onoff');
  document.querySelectorAll('.pid-only').forEach(el => el.classList.toggle('hidden', !showPidLike));
  document.querySelectorAll('.manual-only').forEach(el => el.classList.toggle('hidden', mode !== 'manual'));
}

modeEl.addEventListener('change', updateModeVisibility);
updateModeVisibility();

document.getElementById('applyConfig').addEventListener('click', async () => {
  const mode = modeEl.value;
  const coolerSpeed = Number(coolerSpeedEl.value);
  try {
    let resp;
    if (mode === 'pid') {
      const node = Number(nodeEl.value);
      const targetTemp = Number(targetTempEl.value);
      resp = await fetch('/api/config/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode, node, targetTemp, coolerSpeed })
      }).then(r => r.json());
    } else if (mode === 'onoff') {
      const node = Number(nodeEl.value);
      const targetTemp = Number(targetTempEl.value);
      resp = await fetch('/api/config/onoff', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode, node, targetTemp, coolerSpeed })
      }).then(r => r.json());
    } else {
      const pwmPercent = Number(pwmPercentEl.value);
      resp = await fetch('/api/config/manual', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode, pwmPercent, coolerSpeed })
      }).then(r => r.json());
    }
    setPre('configResult', resp);
  } catch (e) {
    setPre('configResult', { error: String(e) });
  }
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
    const s = await fetchJSON('/api/sensors');
    history.push({ t: Date.now(), temps: s.temperatures || [], ambient: s.ambient, control_pct: s.control_pct });
    if (history.length > MAX_POINTS) history.shift();
    drawChart();
  } catch (e) {
    console.error('Error al consultar /api/sensors:', e);
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
