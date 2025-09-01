async function fetchJSON(path) {
  const res = await fetch(path);
  if (!res.ok) throw new Error(`${res.status} ${res.statusText}`);
  return res.json();
}

function setPre(id, data) {
  document.getElementById(id).textContent = JSON.stringify(data, null, 2);
}

document.getElementById('btnHealth').addEventListener('click', async () => {
  try { setPre('health', await fetchJSON('/api/health')); }
  catch (e) { setPre('health', { error: String(e) }); }
});

document.getElementById('btnSensors').addEventListener('click', async () => {
  try { setPre('sensors', await fetchJSON('/api/sensors')); }
  catch (e) { setPre('sensors', { error: String(e) }); }
});

document.getElementById('btnMock').addEventListener('click', async () => {
  try { setPre('mock', await fetchJSON('/api/mock')); }
  catch (e) { setPre('mock', { error: String(e) }); }
});

// Show effective backend (filled by nginx proxy to /api/*)
document.getElementById('backendUrl').textContent = 'API: proxied by Nginx at /api/*';

