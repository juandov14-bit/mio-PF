const express = require('express');
const app = express();
app.use(express.json());
const PORT = process.env.PORT || 3000;

app.get('/api/health', (_req, res) => {
  res.json({ status: 'ok', mock: true });
});

app.get('/api/mock', (_req, res) => {
  res.json({
    temperatures: [23.4, 24.1, 24.8, 25.2, 22.9],
    heater_w: 1.23,
    cooler_pct: 66,
  });
});

// Por ahora /api/sensors devuelve datos de prueba también
app.get('/api/sensors', (_req, res) => {
  res.json({ temperatures: [23.4, 24.1, 24.8, 25.2, 22.9] });
});

// Estado de configuración en memoria (mock)
const state = {
  mode: 'pid', // 'pid' | 'manual'
  node: 0,
  targetTemp: 30,
  coolerSpeed: 1,
  pwmPercent: 0,
};

app.get('/api/config', (_req, res) => res.json(state));

app.post('/api/config/control', (req, res) => {
  const { mode, node, targetTemp, coolerSpeed } = req.body || {};
  if (mode !== 'pid') return res.status(400).json({ error: 'mode must be pid' });
  if (!(Number.isInteger(node) && node >= 0 && node <= 4)) return res.status(400).json({ error: 'node 0..4' });
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

app.listen(PORT, () => {
  console.log(`Mock backend listening on :${PORT}`);
});
