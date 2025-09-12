#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <OneWire.h>
#include <OneWireNg.h>
#include <OneWireNg_BitBang.h>
#include <OneWireNg_Config.h>
#include <OneWireNg_CurrentPlatform.h>

#include <DallasTemperature.h>

#include "API_Resistor.h"
#include "API_Sensors.h"
#include "API_MyTimer.h"
#include "API_Control_PID.h"
#include "API_HttpServer.h"


API_Resistor      Qin;
API_Sensors       Temperature;
API_MyTimer       MyTimer;
API_Control_PID   PID;


bool exec_option();
bool exec_run();
bool exec_stop();
void send_data();
void set_cooler_pwm(int percent);

// define COOLER cooler
#define COOLER_PIN          4
#define COOLER_FREQ         5000
#define COOLER_CHANNEL      1
#define COOLER_RESOLUTION   8
#define T_SAMPLE            1000
#define RANDOM_MINUTES_MIN  1
#define RANDOM_MINUTES_MAX  10

// Se definen el orden de los sensores
// Mapeo claro: 0 = ambiente, 1..4 = barra
enum sensor_order{Troom = 0, Tnode1 = 1, Tnode2 = 2, Tnode3 = 3, Tnode4 = 4};


// define PARAMETROS DE CONTROL
#define PID_KP        0.354456662137341
#define PID_KI        0.000259315631755468
#define PID_TS        1.0
#define PID_U_MAX     2.32
#define PID_REF       30

PID_config PID_data = {0,0,PID_REF,PID_KP,PID_KI,PID_TS,1,0,PID_U_MAX,0};


File dataFile; // Objeto para el archivo de datos
String dataBuffer = ""; // Búfer para almacenar datos antes de escribir al archivo

enum Estado {
  coolerLevel,
  modoSeleccion,
  modoFijoResistencia,
  nodoSelect,
  tempConfig,
  runFijo,
  runPID
  };

Estado estadoActual = coolerLevel; // Estado inicial
int nodoSeleccionado = 1; // 1..4
int porcentajeResistencia = 0; // % PWM resistencia (modo fijo)
int porcentajeCooler = 100; // % cooler

// Estado controlado vía API (ver API_HttpServer.cpp)
volatile bool  g_running = false;
volatile int   g_mode = 0;            // 0=fijo, 1=pid
volatile int   g_selectedNode = 1;    // 1..4
volatile float g_setpoint = PID_REF;  // °C
volatile int   g_fixedPercent = 0;    // 0..100
volatile int   g_coolerPercent = 100; // 0..100
  
void init_cooler(){
  ledcSetup(COOLER_CHANNEL, COOLER_FREQ, COOLER_RESOLUTION);
  ledcAttachPin(COOLER_PIN, COOLER_CHANNEL);  
  set_cooler_pwm(100);   // init cooler at 100%
}
void set_cooler_pwm(int percent){
  ledcWrite(COOLER_CHANNEL, (percent/100.0)*255);  
}

bool exec_square_cooler(int cooler_speed) {
  static bool my_state = HIGH;
  if (cooler_speed == 0) {
    set_cooler_pwm(0); // Apagar el cooler
    my_state = LOW;
  } else if (cooler_speed == 1) {
    set_cooler_pwm(33); // Configurar el PWM al 33% para velocidad baja (low)
    my_state = HIGH;
  } else if (cooler_speed == 2) {
    set_cooler_pwm(66); // Configurar el PWM al 66% para velocidad media (mid)
    my_state = HIGH;
  } else if (cooler_speed == 3) {
    set_cooler_pwm(100); // Configurar el PWM al 100% para velocidad alta (high)
    my_state = HIGH;
  } else {
    // Opción no válida, apagar el cooler
    set_cooler_pwm(0);
    my_state = LOW;
  }
  return true;
}

int selectedNode;
int Q_data = 200;


void setup() {
  // start serial port
  Serial.begin(115200);
  Serial.println("[BOOT] Setup start");
  // Inicializa sensores ahora que Serial está listo
  Temperature.init(true);
  Serial.println("[BOOT] Sensors init done");
  init_cooler(); // start cooler  100 %
  set_cooler_pwm(g_coolerPercent);
  Qin.set_pwm(0); // power OFF resistor 0%
  PID.configure(PID_data);  // configura el control  
  // Inicia API HTTP en modo AP con endpoints
  httpServerSetup();
  Serial.println("[BOOT] HTTP server ready");
  }
  
void loop() { 
  // Servicio HTTP
  httpServerLoop();

  // Aplicar valores recibidos por API
  set_cooler_pwm(g_coolerPercent);
  porcentajeResistencia = g_fixedPercent;
  nodoSeleccionado = g_selectedNode;

  // Si RUN está activo, forzamos el estado de ejecución según modo
  if (g_running) {
    estadoActual = (g_mode == 1) ? runPID : runFijo;
  }
  switch (estadoActual) {
    
    case coolerLevel:
      // Esperar la entrada del usuario y configurar el porcentaje de potencia del cooler
      // Entrada por Serial deshabilitada; control via API
      if (false && Serial.available()) {
        String userInput = Serial.readStringUntil('\n');
        int value = userInput.toInt();
        if (value >= 0 && value < 4) {
          int velocidadCooler = value;
          Serial.println("Velocidad del cooler configurada: Nivel" + String(velocidadCooler) + "de Velocidad");
          exec_square_cooler(velocidadCooler);
          estadoActual = modoSeleccion; // Cambiar al estado de eleccion de modo
        } else {
          Serial.println("Valor inválido. Intente de nuevo.");
        }
      }
      break;

    case modoSeleccion:
      Serial.println("Seleccione el modo de operación:");
      Serial.println("1. Modo Fijo");
      Serial.println("2. Modo PID");
      // Entrada por Serial deshabilitada; control via API
      if (false && Serial.available()) {
        String userInput = Serial.readStringUntil('\n');
        if (userInput.equals("1")) {
          estadoActual = modoFijoResistencia;
          Serial.println("Modo Fijo - Configuración:");
          Serial.println("Ingrese el porcentaje de PWM de la resistencia (0-100):");
        } else if (userInput.equals("2")) {
          estadoActual = nodoSelect;            //al estado seleccion de nodo
          Serial.println("Modo PID - Configuración:");
          Serial.println("Ingrese el nodo de acción (1-4):");
        } else {
          Serial.println("Selección inválida. Intente de nuevo.");
        }
      }
      break;

    case modoFijoResistencia:
      // Esperar la entrada del usuario y configurar el porcentaje de PWM de la resistencia
      if (Serial.available()) {
        String userInput = Serial.readStringUntil('\n');
        int value = userInput.toInt();
        if (value >= 0 && value <= 100) {
          porcentajeResistencia = value;
          Serial.println("PWM de la resistencia configurado: " + String(porcentajeResistencia) + "%");
          estadoActual = runFijo;   // correr con valores fijos
          } else {
          Serial.println("Valor inválido. Intente de nuevo.");
        }
      }
      break;

    
    case nodoSelect:
       if (false && Serial.available()) {
        String userInput = Serial.readStringUntil('\n');
        selectedNode = userInput.toInt();
        // Solo permitir nodos de la barra (1..4). 0 es el sensor ambiente.
        if (selectedNode >= 1 && selectedNode <= 4) {
          // Configurar el nodo seleccionado
          nodoSeleccionado = selectedNode;
          Serial.println("Nodo seleccionado: " + String(nodoSeleccionado));
          estadoActual = tempConfig; // Cambiar al estado de seleccion de temperatura
        } else {
          Serial.println("Selección inválida. Intente de nuevo.");
        }
    }
    break;

    case tempConfig:
      Serial.println("Ingrese la temperatura deseada:");
      // Entrada por Serial deshabilitada; control via API
      if (false && Serial.available()) {
        String userInput = Serial.readStringUntil('\n');
        float desiredTemp = userInput.toFloat();
        if (desiredTemp >= 18 && desiredTemp <= 40) {
          // Configurar la temperatura deseada
          float temperaturaConfigurada = desiredTemp;
          Serial.println("Temperatura configurada: " + String(temperaturaConfigurada));
          estadoActual = runPID;    //correr con PiD
        } else {
          Serial.println("Temperatura inválida. Intente de nuevo.");
        }
      }
      
      break;

    case runPID: { // control PID no-bloqueante
      // Si no está en RUN, salir a idle
      if (!g_running) { Qin.set_pwm(0); estadoActual = coolerLevel; break; }

      // Ejecutar tareas periódicas sin bloquear
      static bool started = false;
      static unsigned long last_step_ms = 0;
      if (!started) { MyTimer.restart(); started = true; }

      // Telemetría y lecturas periódicas
      send_data();

      // Paso de control a 1 Hz (T_SAMPLE)
      unsigned long now = millis();
      if (now - last_step_ms >= T_SAMPLE) {
        last_step_ms = now;
        float y = Temperature.getTemperatureId(nodoSeleccionado);
        float u = 43.1034f * PID.update(y);
        Qin.set_pwm(u);
      }

      // Si se recibe STOP por API, salir limpiamente
      if (!g_running) { Qin.set_pwm(0); started = false; estadoActual = coolerLevel; }
      break;
    }
    
    case runFijo: { // control fijo no-bloqueante
      if (!g_running) { Qin.set_pwm(0); estadoActual = coolerLevel; break; }

      static bool started = false;
      static unsigned long last_step_ms = 0;
      if (!started) { MyTimer.restart(); started = true; }

      // Telemetría y lecturas periódicas
      send_data();

      // Actualiza salida a 1 Hz (T_SAMPLE) para evitar saturar el bus
      unsigned long now = millis();
      if (now - last_step_ms >= T_SAMPLE) {
        last_step_ms = now;
        Qin.set_pwm(porcentajeResistencia);
      }

      if (!g_running) { Qin.set_pwm(0); started = false; estadoActual = coolerLevel; }
      break;
    }
  }
}

/**************************************************************/
// init: Functions of section loop
/**************************************************************/

bool configure_Q(){
  int auxQ = 200;
  if(Serial.available()) {
      String string_data = Serial.readStringUntil('\n');
      auxQ = string_data.toInt();
      if( auxQ>=0 && auxQ<=100 ){
        Q_data = auxQ;
        Serial.print(Q_data);
        Serial.println(" ....");       
        return true;
      } else {
        Serial.println("Valor incorrecto");
        Serial.print("Ingrese el % de calor deseado (de 0 a 100): ");
      }
  }    
  return false;
}



bool exec_run(){
  // RUN controlado por API
  if (g_running) return true;
  if(Serial.available()) {
    String string_data = Serial.readStringUntil('\n');
    if (string_data.equals("RUN")) return true;
  }
  return false;
}

bool exec_option(){
  // No bloquear nunca el loop por entrada Serial
  return true;
}
  

bool exec_stop(){
  if (!g_running) { Qin.set_pwm(0); return true; }
  if(Serial.available()) {
    String string_data = Serial.readStringUntil('\n');
    if (string_data.equals("STOP")) { Qin.set_pwm(0); return true; }
  }
  return false;
}


bool exec_square(float Ts_minutes){
  if( MyTimer.get_minutes()>=Ts_minutes ){
    if( Qin.get_heat()==0 ) Qin.set_pwm(Q_data);
    else  Qin.set_pwm(0);
    MyTimer.restart();
    return true;
  }
  return false;
}


void exec_square_random(){
  static float Ts_minutes = random(RANDOM_MINUTES_MIN*100,RANDOM_MINUTES_MAX*100)/100.0;
  if( exec_square(Ts_minutes) ){
    Ts_minutes = random(RANDOM_MINUTES_MIN*100,RANDOM_MINUTES_MAX*100)/100.0;
  }
}


void send_data(){
  static long t1=millis();
  static float temp_nodos[DEVICES_CONNECT];
  
  if( (millis()-t1)>=T_SAMPLE ) {
    t1 = millis();
    
    // Test Sensors
    Temperature.getTemperatures(temp_nodos);
    Serial.print(temp_nodos[Tnode1]); Serial.print(" ");
    Serial.print(temp_nodos[Tnode2]); Serial.print(" ");
    Serial.print(temp_nodos[Tnode3]); Serial.print(" ");
    Serial.print(temp_nodos[Tnode4]); Serial.print(" ");
    
    // Test Resistor: measurement heat
    Serial.print(Qin.get_heat());     Serial.print(" ");
    Serial.println(temp_nodos[Troom]);
  }          

}
