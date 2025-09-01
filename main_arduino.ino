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
enum sensor_order{Troom = 1, Tnode1 = 0, Tnode2 = 2, Tnode3 = 3, Tnode4 = 4};


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

Estado estadoActual = coolerLevel; // Estado inicial: modoSeleccion
int nodoSeleccionado = 0; // Nodo seleccionado inicialmente
int porcentajeResistencia = 0; // Porcentaje de PWM de la resistencia
int porcentajeCooler = 0; // Porcentaje de potencia del cooler
  
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
  Serial.println("LU");    
  init_cooler(); // start cooler  100 %
  Qin.set_pwm(0); // power OFF resistor 0%
  PID.configure(PID_data);  // configura el control  
  }
  
void loop() { 
  Serial.println("LLU"); 
  switch (estadoActual) {
    
    case coolerLevel:
      // Esperar la entrada del usuario y configurar el porcentaje de potencia del cooler
      for (int i = 0; i < 50; i++) {  // Limpia la pantalla
        Serial.println();
      }
      Serial.println("elija la velocidad del cooler:(0-3)");
      while(!exec_option());
      if (Serial.available()) {
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
      // Esperar la selección del usuario entre modo fijo y modo PID
      while(!exec_option());
      if (Serial.available()) {
        String userInput = Serial.readStringUntil('\n');
        if (userInput.equals("1")) {
          estadoActual = modoFijoResistencia;
          Serial.println("Modo Fijo - Configuración:");
          Serial.println("Ingrese el porcentaje de PWM de la resistencia (0-100):");
        } else if (userInput.equals("2")) {
          estadoActual = nodoSelect;            //al estado seleccion de nodo
          Serial.println("Modo PID - Configuración:");
          Serial.println("Ingrese el nodo de acción (0-4):");
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
       if (Serial.available()) {
        String userInput = Serial.readStringUntil('\n');
        selectedNode = userInput.toInt();
        if (selectedNode >= 0 && selectedNode < DEVICES_CONNECT) {
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
      // Esperar la entrada del usuario y configurar la temperatura deseada
      while(!exec_option());
      if (Serial.available()) {
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

    case runPID:// wait until "RUN"
      Serial.println("Espera por 'RUN' para comenzar");
      while( !exec_run() );
      MyTimer.restart();
      
      //corriendo hasta "stop"
      while(!exec_stop()){
        send_data();
        //exec_square(1);
        //exec_square_random();    
        Qin.set_pwm(43.1034*PID.update( Temperature.getTemperatureId(selectedNode) ));      
        //exec_square_cooler(10);
      }
      estadoActual = coolerLevel; // Cambiar al estado de selección de velocidad de cooler
      break;
    
    case runFijo:// wait until "RUN"
      Serial.println("Espera por 'RUN' para comenzar");
      while( !exec_run() );
      MyTimer.restart();
      
      //corriendo hasta "stop"
      while(!exec_stop()){
        send_data();
        //exec_square(1);
        //exec_square_random();    
        Qin.set_pwm(porcentajeResistencia);      
        //exec_square_cooler(10);
      }
      estadoActual = coolerLevel; // Cambiar al estado de selección de velocidad de cooler
      break;
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
  if(Serial.available()) {
    String string_data = Serial.readStringUntil('\n');
    if (string_data.equals("RUN")) return true;
  }
  return false;
}

bool exec_option(){
  if(Serial.available()) {
    return true;
  }
  return false;
}
  

bool exec_stop(){
  if(Serial.available()) {
    String string_data = Serial.readStringUntil('\n');
    if (string_data.equals("STOP")) {              
      Qin.set_pwm(0); // power OFF resistor 0%
      return true;
    } 
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
