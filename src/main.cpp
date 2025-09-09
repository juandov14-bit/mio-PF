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

// Estado de control expuesto a la API
enum ControlMode { MODE_FIJO = 0, MODE_PID = 1 };
volatile bool  g_running = false;
volatile int   g_mode = MODE_FIJO;
volatile int   g_selectedNode = 1;   // 1..4
volatile float g_setpoint = PID_REF; // °C
volatile int   g_fixedPercent = 0;   // 0..100
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

  // Aplicar PWM del cooler (según API)
  set_cooler_pwm(g_coolerPercent);

  // Control no bloqueante
  static float lastConfiguredSetpoint = PID_REF;
  if (g_running) {
    if (g_mode == MODE_PID && fabs(g_setpoint - lastConfiguredSetpoint) > 1e-6) {
      PID_data.y_ref = g_setpoint;
      PID.configure(PID_data);
      lastConfiguredSetpoint = g_setpoint;
      Serial.println(String("[RUN] Setpoint actualizado a ") + String(g_setpoint, 1));
    }

    float y = Temperature.getTemperatureId(g_selectedNode);
    float u_percent = 0.0f;
    if (g_mode == MODE_PID) {
      float u = PID.update(y);
      u_percent = 43.1034f * u;
    } else {
      u_percent = g_fixedPercent;
    }
    if (u_percent < 0) u_percent = 0; if (u_percent > 100) u_percent = 100;
    Qin.set_pwm((int)u_percent);

    send_data();
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


// Entradas por Serial eliminadas: control por API


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
    float heatW = Qin.get_heat();
    Serial.print("[DATA] N1="); Serial.print(temp_nodos[Tnode1],2);
    Serial.print(" N2="); Serial.print(temp_nodos[Tnode2],2);
    Serial.print(" N3="); Serial.print(temp_nodos[Tnode3],2);
    Serial.print(" N4="); Serial.print(temp_nodos[Tnode4],2);
    Serial.print(" Room="); Serial.print(temp_nodos[Troom],2);
    Serial.print(" HeatW="); Serial.print(heatW,3);
    Serial.print(" Mode="); Serial.print(g_mode==0?"fixed":"pid");
    Serial.print(" Run="); Serial.print(g_running?"1":"0");
    Serial.print(" Node="); Serial.print(g_selectedNode);
    Serial.print(" SP="); Serial.print(g_setpoint,1);
    Serial.print(" Fix%="); Serial.print(g_fixedPercent);
    Serial.print(" Cool%="); Serial.println(g_coolerPercent);
  }          

}
