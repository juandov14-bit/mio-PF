#ifndef API_HTTP_SERVER_H
#define API_HTTP_SERVER_H

#include <Arduino.h>

// Inicializa WiFi (modo AP) y el servidor HTTP con rutas
void httpServerSetup();

// Atiende peticiones entrantes; llamarlo frecuentemente en loop()
void httpServerLoop();

#endif

