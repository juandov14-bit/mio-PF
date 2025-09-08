const express = require('express');
const app = express();
app.use(express.json());

// Salud
app.get('/api/health', (_req, res) => {
  res.json({ status: 'ok', mock: true });
});

// Mock fijo
app.get('/api/mock', (_req, res) => {
  res.json({
    temperatures: [23.4, 24.1, 24.8, 25.2, 22.9],
    heater_w: 1.23,
    cooler_pct: 66,
  });
});

// Estado + simulación
const state = {
  mode: 'pid',      // 'pid' | 'onoff' | 'manual'
  node: 0,          // 0..4
  targetTemp: 30,   // °C
  coolerSpeed: 1,   // 0..3
  pwmPercent: 0,    // 0..100
  ambient: 22.0,    // °C ambiente
  temps: [22, 22, 22, 22], // 4 sensores sobre la barra
  ambient_meas: 22.0,      // lectura ambiente simulada
  heater_w: 0,             // 0..10 W aprox (para el gráfico)
  control_pct: 0,          // 0..100% (acción de control)
  onoff_on: true,          // estado ON/OFF (relay virtual)
};

// Simulador: rampas casi lineales con pequeño acople entre nodos
const SIM_PERIOD_MS = 1000;
const MAX_W = 10;
// Aumenta pendientes para curvas más visibles
const RAMP_MAX = 0.8; // °C/s máximo en el nodo objetivo (antes 0.25)

function clamp(x, a, b) { return Math.max(a, Math.min(b, x)); }

function stepSim() {
  const { mode, node, targetTemp, coolerSpeed, pwmPercent, ambient } = state;

  // Actuación normalizada 0..1
  let u_norm = 0;
  if (mode === 'pid') {
    const e = targetTemp - state.temps[node];
    const kp = 0.18; // proporcional más agresivo
    u_norm = clamp(kp * e, 0, 1);
  } else if (mode === 'onoff') {
    const Tn = state.temps[node];
    const H = 1.0; // histéresis total 1.0°C
    if (Tn >= targetTemp + H/2) state.onoff_on = false;
    else if (Tn <= targetTemp - H/2) state.onoff_on = true;
    u_norm = state.onoff_on ? 1 : 0; // misma potencia siempre cuando está ON
  } else {
    u_norm = clamp((pwmPercent || 0) / 100, 0, 1);
  }
  state.control_pct = Math.round(u_norm * 100);

  // Cooler 0..3 mapeado a 0/33/66/100 %
  const mapCool = [0, 33, 66, 100];
  const cooler_pct = mapCool[clamp(coolerSpeed|0, 0, 3)];
  const cooler_norm = cooler_pct / 100; // 0..1

  // Efecto del cooler sobre la eficiencia
  const coolerFactor = 1 - 0.40 * cooler_norm; // 1..0.60 (ligeramente menos restrictivo)
  state.heater_w = u_norm * MAX_W * coolerFactor;

  // Rampa casi lineal en el nodo objetivo
  const Tn = state.temps[node];
  let dTn = 0;
  if (mode === 'pid') {
    const e = targetTemp - Tn;
    const dir = Math.sign(e) || 0;
    // Magnitud de rampa depende de u_norm pero con piso mínimo para que se vea movimiento
    const ramp = RAMP_MAX * (0.25 + 0.75 * u_norm) * coolerFactor;
    dTn = dir * Math.min(Math.abs(e), ramp);
  } else {
    // En manual, rampa positiva según pwm y efecto del cooler
    const ramp = RAMP_MAX * u_norm * coolerFactor;
    dTn = ramp - 0.02 * (Tn - ambient); // leve retorno a ambiente
  }

  // Actualiza nodo objetivo
  state.temps[node] = Tn + dTn + (Math.random() - 0.5) * 0.06; // pequeño ruido

  // Nodos restantes: se acercan suavemente al nodo y al ambiente
  for (let i = 0; i < state.temps.length; i++) {
    if (i === node) continue;
    const Ti = state.temps[i];
    const towardNode = 0.20 * (state.temps[node] - Ti); // mayor acople
    const towardAmb = 0.03 * (ambient - Ti);
    const coolLoss = 0.02 * cooler_norm * 3; // 0..0.06
    state.temps[i] = Ti + towardNode + towardAmb - coolLoss + (Math.random() - 0.5) * 0.06;
  }

  // Lectura de ambiente: tiende a 'ambient' con leve ruido
  const Ta = state.ambient_meas;
  state.ambient_meas = Ta + 0.05 * (ambient - Ta) + (Math.random() - 0.5) * 0.05;
}

setInterval(stepSim, SIM_PERIOD_MS);

// Endpoints
app.get('/api/sensors', (_req, res) => {
  res.json({
    temperatures: state.temps.map(v => Number(v.toFixed(2))), // 4 sensores
    ambient: Number(state.ambient_meas.toFixed(2)),
    heater_w: Number(state.heater_w.toFixed(2)),
    control_pct: state.control_pct,
    cooler_pct: [0,33,66,100][clamp(state.coolerSpeed|0,0,3)],
    mode: state.mode,
    node: state.node,
    targetTemp: state.targetTemp,
    coolerSpeed: state.coolerSpeed,
  });
});

app.get('/api/config', (_req, res) => res.json(state));

app.post('/api/config/control', (req, res) => {
  const { mode, node, targetTemp, coolerSpeed } = req.body || {};
  if (mode !== 'pid') return res.status(400).json({ error: 'mode must be pid' });
  if (!(Number.isInteger(node) && node >= 0 && node <= 3)) return res.status(400).json({ error: 'node 0..3' });
  if (!(typeof targetTemp === 'number' && targetTemp >= 10 && targetTemp <= 80)) return res.status(400).json({ error: 'targetTemp 10..80' });
  if (!(Number.isInteger(coolerSpeed) && coolerSpeed >= 0 && coolerSpeed <= 3)) return res.status(400).json({ error: 'coolerSpeed 0..3' });
  Object.assign(state, { mode, node, targetTemp, coolerSpeed });
  return res.json({ ok: true, state });
});

app.post('/api/config/onoff', (req, res) => {
  const { mode, node, targetTemp, coolerSpeed } = req.body || {};
  if (mode !== 'onoff') return res.status(400).json({ error: 'mode must be onoff' });
  if (!(Number.isInteger(node) && node >= 0 && node <= 3)) return res.status(400).json({ error: 'node 0..3' });
  if (!(typeof targetTemp === 'number' && targetTemp >= 10 && targetTemp <= 80)) return res.status(400).json({ error: 'targetTemp 10..80' });
  if (!(Number.isInteger(coolerSpeed) && coolerSpeed >= 0 && coolerSpeed <= 3)) return res.status(400).json({ error: 'coolerSpeed 0..3' });
  Object.assign(state, { mode, node, targetTemp, coolerSpeed });
  return res.json({ ok: true, state });
});

app.post('/api/config/manual', (req, res) => {
  const { mode, pwmPercent, coolerSpeed } = req.body || {};
  if (mode !== 'manual') return res.status(400).json({ error: 'mode must be manual' });
  if (!(Number.isInteger(coolerSpeed) && coolerSpeed >= 0 && coolerSpeed <= 3)) return res.status(400).json({ error: 'coolerSpeed 0..3' });
  if (!(typeof pwmPercent === 'number' && pwmPercent >= 0 && pwmPercent <= 100)) return res.status(400).json({ error: 'pwmPercent 0..100' });
  Object.assign(state, { mode, pwmPercent, coolerSpeed });
  return res.json({ ok: true, state });
});

const PORT = process.env.PORT || 3000;
app.listen(PORT, () => {
  console.log(`Mock backend listening on :${PORT}`);
});
