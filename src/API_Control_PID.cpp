#include "API_Control_PID.h"

API_Control_PID::API_Control_PID() {
  API_Control_PID::init();
}

void API_Control_PID::init() {
  PID_data.u = 0;
  PID_data.y = 0;
  PID_data.y_ref = 0;

  PID_data.KP = 0;
  PID_data.KI = 0;
  PID_data.Ts = 0;
  PID_data.Kb = 0;  
  PID_data.t_integ = 0;  

  PID_data.u_max = 0;  
  PID_data.u_min = 0;

  isConfigure = false;
}

void API_Control_PID::configure(PID_config PID_data_in){
  PID_data = PID_data_in;
  isConfigure = true;  
}

float API_Control_PID::update(float data_in){
  PID_data.y = data_in;

  // actualiza salida
  PID_data.u = PID_data.KP * ( PID_data.Kb * PID_data.y_ref - PID_data.y) + PID_data.t_integ;

  // saturacion de salida
  if (PID_data.u > PID_data.u_max) {
    PID_data.u = PID_data.u_max;
    } else if (PID_data.u < PID_data.u_min) {
    PID_data.u = PID_data.u_min;
  }

  // actualiza proximo termino integrador
  PID_data.t_integ += PID_data.KI * PID_data.Ts * ( PID_data.y_ref - PID_data.y);
  
  return PID_data.u;
}

