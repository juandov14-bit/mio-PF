#ifndef API_Control_PID_h
#define API_Control_PID_h
 
#include "Arduino.h"

struct PID_config{
      // variables del proceso control
      float u; float y; float y_ref;
      // parametros del control
      float KP; float KI; float Ts; float Kb; float t_integ;
      // saturacion 
      float u_max; float u_min;    
    };
    
class API_Control_PID {
public:
    API_Control_PID();
    void configure(PID_config PID_data_in);
    float update(float data_in);
  
private:
    PID_config PID_data;
    bool isConfigure;

    void init();

};

#endif
