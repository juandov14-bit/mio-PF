const express = require('express');
const app = express();
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

app.listen(PORT, () => {
  console.log(`Mock backend listening on :${PORT}`);
});

