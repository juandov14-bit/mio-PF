#ifndef API_Resistor_h
#define API_Resistor_h
 
#include "Arduino.h"

// define output resistor PWM
#define RESISTOR_PIN_OUT     27
#define RESISTOR_FREQ        1000
#define RESISTOR_CHANNEL     0
#define RESISTOR_RESOLUTION  8

// define analog input mean voltage
#define HEAT_PIN_AN_IN       32


// define data of resistor & conversion
#define RESISTOR_VALUE          10.3 // OHM (at 20°C)
#define READ_TO_REAL_VOLTAGE    1.4818 // 3.3V(read) --> 4.89V (real)

class API_Resistor {
public:
    API_Resistor();
    void init();
    void set_pwm(int percent);
    float get_heat();
    int get_set_pwm_percent() { return __actual_set_pwm_percent; }
  
private:
    int __pin_out;
    int __channel;
    int __freq;
    int __resolution;
    int __pin_analog_in;

    int __actual_set_pwm_percent;
    float __last_calc_heat;

};
 
#endif
