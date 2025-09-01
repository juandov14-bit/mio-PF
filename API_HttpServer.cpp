#include "API_HttpServer.h"

#include <WiFi.h>
#include <WebServer.h>

#include "API_Sensors.h"

// Usa el objeto global de sensores
extern API_Sensors Temperature;

// Config AP por defecto (puedes cambiarlo)
#ifndef WIFI_AP_SSID
#define WIFI_AP_SSID "PF-Test"
#endif
#ifndef WIFI_AP_PASS
#define WIFI_AP_PASS "12345678"
#endif

static WebServer server(80);

static void sendJson(const String& body, int code = 200) {
  server.send(code, "application/json", body);
}

static void handleHealth() {
  sendJson("{\"status\":\"ok\"}");
}

static void handleMock() {
  // Datos de prueba estáticos/deterministas
  String json = "{";
  json += "\"temperatures\":[23.4,24.1,24.8,25.2,22.9],";
  json += "\"heater_w\":1.23,";
  json += "\"cooler_pct\":66";
  json += "}";
  sendJson(json);
}

static void handleSensors() {
  float temps[DEVICES_CONNECT] = {0};
  Temperature.getTemperatures(temps);
  String json = "{";
  json += "\"temperatures\":[";
  for (int i=0;i<DEVICES_CONNECT;i++) {
    if (i) json += ",";
    json += String(temps[i], 2);
  }
  json += "]";
  json += "}";
  sendJson(json);
}

void httpServerSetup() {
  // Levanta un AP para pruebas
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);

  // Rutas API
  server.on("/api/health", HTTP_GET, handleHealth);
  server.on("/api/mock", HTTP_GET, handleMock);
  server.on("/api/sensors", HTTP_GET, handleSensors);

  // Raíz sencilla
  server.on("/", HTTP_GET, [](){
    server.send(200, "text/plain",
                String("ESP32 API running. Connect to AP: ") + WIFI_AP_SSID +
                ", endpoints: /api/health, /api/mock, /api/sensors");
  });

  server.begin();
}

void httpServerLoop() {
  server.handleClient();
}

