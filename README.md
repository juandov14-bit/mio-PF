Proyecto PF – Simulación sin hardware

Descripción
- Este repo incluye un frontend simple y un backend mock para simular la planta sin conectar el ESP32.
- La simulación genera temperaturas con rampas casi lineales, más que suficientes para validar la UI/flujo.

Cómo correr la simulación
1) Requisitos: Docker + Docker Compose.
2) Levantar servicios:
   - `docker compose up -d`
3) Abrir el frontend:
   - http://localhost:8080

Endpoints relevantes (mock backend)
- `GET /api/health`: estado del mock.
- `GET /api/sensors`: devuelve `{ temperatures:[T0..T4], heater_w }` con datos simulados.
- `POST /api/config/control`: cuerpo `{ mode:"pid", node, targetTemp, coolerSpeed }` ajusta consigna y nodo.
- `POST /api/config/manual`: cuerpo `{ mode:"manual", pwmPercent, coolerSpeed }` activa modo manual.

Notas de simulación
- Nodo seleccionado sube/baja con rampa casi lineal (≈0.25 °C/s máximo), con leve ruido.
- Cooler reduce la pendiente efectiva.
- Nodos no seleccionados siguen suavemente al nodo objetivo y al ambiente.

Usar con un ESP32 real
- Si querés apuntar el frontend a un ESP32 real, levantá solo el `frontend` y exportá `BACKEND_URL=http://<ip-del-esp>` antes de `docker compose up -d`.

