#include "API_HttpServer.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ctype.h>

#include "API_Sensors.h"
#include "API_Resistor.h"

// Usa objetos globales
extern API_Sensors Temperature;
extern API_Resistor Qin;
// Estado de control (definido en main.cpp)
extern volatile bool  g_running;
extern volatile int   g_mode;      // 0 fijo, 1 pid
extern volatile int   g_selectedNode; // 1..4
extern volatile float g_setpoint;  // °C
extern volatile int   g_fixedPercent; // 0..100
extern volatile int   g_coolerPercent; // 0..100

// Config STA/AP por defecto (puedes cambiarlos por build_flags)
#ifndef WIFI_STA_SSID
#define WIFI_STA_SSID ""
#endif
#ifndef WIFI_STA_PASS
#define WIFI_STA_PASS ""
#endif

// Fallback AP si STA falla o no está configurado
#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID "PF-Test"
#endif
#ifndef WIFI_AP_PASS
#define WIFI_AP_PASS "12345678"
#endif

static WebServer server(80);

static void sendJson(const String& body, int code = 200) {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(code, "application/json", body);
}

static void handleHealth() {
  sendJson("{\"status\":\"ok\"}");
}

// Endpoints de control
static void handleRunStart() { g_running = true; Serial.println("[API] RUN iniciado"); sendJson("{\"ok\":true}"); }
static void handleRunStop()  { g_running = false; Qin.set_pwm(0); Serial.println("[API] RUN detenido"); sendJson("{\"ok\":true}"); }
static void handleMode() {
  String t = server.arg("type");
  if (t == "pid") g_mode = 1; else g_mode = 0;
  Serial.println(String("[API] mode=") + (g_mode?"pid":"fixed"));
  sendJson("{\"ok\":true}");
}
static void handleNode() {
  int idx = server.arg("index").toInt();
  if (idx>=1 && idx<=4) { g_selectedNode = idx; Serial.println(String("[API] node=") + idx); sendJson("{\"ok\":true}"); }
  else sendJson("{\"error\":\"index 1-4\"}", 400);
}
static void handleSetpoint() {
  float sp = server.arg("temp").toFloat();
  if (sp>=5 && sp<=90) { g_setpoint = sp; Serial.println(String("[API] setpoint=") + String(sp,1)); sendJson("{\"ok\":true}"); }
  else sendJson("{\"error\":\"temp 5-90C\"}", 400);
}
static void handleFixed() {
  int p = server.arg("percent").toInt();
  if (p>=0 && p<=100) { g_fixedPercent = p; Serial.println(String("[API] fixed%=") + p); sendJson("{\"ok\":true}"); }
  else sendJson("{\"error\":\"percent 0-100\"}", 400);
}
static void handleCooler() {
  int p = server.arg("percent").toInt();
  if (p>=0 && p<=100) { g_coolerPercent = p; Serial.println(String("[API] cooler%=") + p); sendJson("{\"ok\":true}"); }
  else sendJson("{\"error\":\"percent 0-100\"}", 400);
}
static void handleState() {
  float temps[DEVICES_CONNECT] = {0};
  Temperature.getTemperatures(temps);
  String json = "{";
  json += "\"running\":"; json += (g_running?"true":"false"); json += ",";
  json += "\"mode\":\""; json += (g_mode?"pid":"fixed"); json += "\",";
  json += "\"node\":"; json += g_selectedNode; json += ",";
  json += "\"setpoint\":"; json += String(g_setpoint,1); json += ",";
  json += "\"fixed_percent\":"; json += g_fixedPercent; json += ",";
  json += "\"cooler_percent\":"; json += g_coolerPercent; json += ",";
  json += "\"temperatures\":{\"room\":"; json += String(temps[0],2); json += ",\"nodes\":[";
  for (int i=1;i<=4;i++){ if(i>1) json+=","; json+=String(temps[i],2);} json += "]},";
  json += "\"heater_w\":"; json += String(Qin.get_heat(),3);
  json += ",\"control_pct\":"; json += Qin.get_set_pwm_percent();
  json += "}";
  sendJson(json);
}

// --- Shims de compatibilidad para rutas antiguas (/api/config/*) ---
static bool parseIntField(const String& src, const char* key, int& out) {
  int pos = src.indexOf(String("\"") + key + "\""); if (pos < 0) return false;
  pos = src.indexOf(':', pos); if (pos < 0) return false; pos++;
  while (pos < (int)src.length() && isspace((unsigned char)src[pos])) pos++;
  int sign = 1; if (pos < (int)src.length() && src[pos]=='-') { sign = -1; pos++; }
  long val = 0; bool any=false; while (pos < (int)src.length() && isdigit((unsigned char)src[pos])) { val = val*10 + (src[pos]-'0'); pos++; any=true; }
  if (!any) return false; out = (int)(sign*val); return true;
}
static bool parseFloatField(const String& src, const char* key, float& out) {
  int pos = src.indexOf(String("\"") + key + "\""); if (pos < 0) return false;
  pos = src.indexOf(':', pos); if (pos < 0) return false; pos++;
  // Avanza hasta la próxima coma o cierre de objeto
  int end = pos;
  while (end < (int)src.length() && src[end] != ',' && src[end] != '}') end++;
  String num = src.substring(pos, end);
  num.trim();
  out = num.toFloat();
  return true;
}
static void handleConfigControl() {
  String body = server.arg("plain");
  int node = 1; float target = 30.0f; int coolerSpeed = 3;
  parseIntField(body, "node", node);
  parseFloatField(body, "targetTemp", target);
  parseIntField(body, "coolerSpeed", coolerSpeed);
  if (node < 1 || node > 4) node = 1;
  int coolerPct = (coolerSpeed<=0?0: coolerSpeed==1?33: coolerSpeed==2?66: 100);
  g_mode = 1; g_selectedNode = node; g_setpoint = target; g_coolerPercent = coolerPct;
  Serial.printf("[Shim] control pid node=%d sp=%.1f cooler%%=%d\n", node, target, coolerPct);
  sendJson("{\"ok\":true}");
}
static void handleConfigOnOff() {
  String body = server.arg("plain");
  int node = 1; float target = 30.0f; int coolerSpeed = 3;
  parseIntField(body, "node", node);
  parseFloatField(body, "targetTemp", target);
  parseIntField(body, "coolerSpeed", coolerSpeed);
  if (node < 1 || node > 4) node = 1;
  int coolerPct = (coolerSpeed<=0?0: coolerSpeed==1?33: coolerSpeed==2?66: 100);
  g_mode = 1; g_selectedNode = node; g_setpoint = target; g_coolerPercent = coolerPct;
  Serial.printf("[Shim] onoff=>pid node=%d sp=%.1f cooler%%=%d\n", node, target, coolerPct);
  sendJson("{\"ok\":true}");
}
static void handleConfigManual() {
  String body = server.arg("plain");
  int pwm = 0; int coolerSpeed = 3;
  parseIntField(body, "pwmPercent", pwm);
  parseIntField(body, "coolerSpeed", coolerSpeed);
  if (pwm<0) pwm=0; if (pwm>100) pwm=100;
  int coolerPct = (coolerSpeed<=0?0: coolerSpeed==1?33: coolerSpeed==2?66: 100);
  g_mode = 0; g_fixedPercent = pwm; g_coolerPercent = coolerPct;
  Serial.printf("[Shim] manual fixed%%=%d cooler%%=%d\n", pwm, coolerPct);
  sendJson("{\"ok\":true}");
}

static bool startSTA(unsigned long timeout_ms = 15000) {
  if (String(WIFI_STA_SSID).length() == 0) {
    Serial.println("[WiFi] STA SSID vacío; se omitirá STA");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_STA_SSID, WIFI_STA_PASS);
  Serial.print("[WiFi] Conectando a STA '" WIFI_STA_SSID "' ");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < timeout_ms) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Conectado. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("[WiFi] No se pudo conectar en STA (timeout)");
  return false;
}

static void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("[WiFi] AP activo: ");
  Serial.print(WIFI_AP_SSID);
  Serial.print(" IP: ");
  Serial.println(ip);
}

// Nota: Se eliminó el endpoint de mock; ahora sólo datos reales

static void handleSensors() {
  float temps[DEVICES_CONNECT] = {0};
  Temperature.getTemperatures(temps);
  String json = "{";
  // Temperaturas separadas: ambiente y nodos 1..4
  json += "\"temperatures\":{\"room\":";
  json += String(temps[0], 2);
  json += ",\"nodes\":[";
  for (int i=1;i<=4;i++) {
    if (i>1) json += ",";
    json += String(temps[i], 2);
  }
  json += "]}";
  // Agrega medición de potencia del calefactor (aprox acción de control)
  json += ",\"heater_w\":" + String(Qin.get_heat(), 3);
  json += "}";
  sendJson(json);
}

void httpServerSetup() {
  // Intenta STA y, si falla, cae a AP
  bool sta_ok = startSTA();
  if (!sta_ok) {
    startAP();
  }

  // Rutas API
  server.on("/api/health", HTTP_GET, handleHealth);
  server.on("/api/sensors", HTTP_GET, handleSensors);
  // Compatibilidad con dashboard anterior
  server.on("/api/config/control", HTTP_POST, handleConfigControl);
  server.on("/api/config/onoff", HTTP_POST, handleConfigOnOff);
  server.on("/api/config/manual", HTTP_POST, handleConfigManual);
  // CORS preflight (OPTIONS)
  server.on("/api/health", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/sensors", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/config/control", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/config/onoff", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/config/manual", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/state", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/run", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/stop", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/mode", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/node", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/setpoint", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/fixed", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/cooler", HTTP_OPTIONS, [](){ sendJson("{}", 204); });
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/run", HTTP_POST, handleRunStart);
  server.on("/api/stop", HTTP_POST, handleRunStop);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/node", HTTP_POST, handleNode);
  server.on("/api/setpoint", HTTP_POST, handleSetpoint);
  server.on("/api/fixed", HTTP_POST, handleFixed);
  server.on("/api/cooler", HTTP_POST, handleCooler);

  // Raíz sencilla
  server.on("/", HTTP_GET, [](){
    String msg = "ESP32 API running. ";
    if (WiFi.getMode() & WIFI_MODE_STA && WiFi.status() == WL_CONNECTED) {
      msg += "STA IP: " + WiFi.localIP().toString();
    } else {
      msg += String("AP SSID: ") + WIFI_AP_SSID + ", IP: " + WiFi.softAPIP().toString();
    }
    msg += ", endpoints: /api/health, /api/sensors, /api/state, /api/run, /api/stop, /api/mode, /api/node, /api/setpoint, /api/fixed, /api/cooler";
    server.send(200, "text/plain", msg);
  });

  server.begin();
}

void httpServerLoop() {
  server.handleClient();
}
