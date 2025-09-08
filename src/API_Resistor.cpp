#include <OneWire.h>
#include <OneWireNg.h>
#include <OneWireNg_BitBang.h>
#include <OneWireNg_Config.h>
#include <OneWireNg_CurrentPlatform.h>

#include "API_Resistor.h"

API_Resistor::API_Resistor() {
    // set defines
    __pin_out = RESISTOR_PIN_OUT;
    __channel = RESISTOR_CHANNEL;
    __freq = RESISTOR_FREQ;
    __resolution = RESISTOR_RESOLUTION;
    __pin_analog_in = HEAT_PIN_AN_IN;
    
    // init object
    API_Resistor::init();

    // set initial conditions
    __actual_set_pwm_percent = 0;      
    __last_calc_heat = 0.0;
}

void API_Resistor::init(){
    ledcSetup(__channel, __freq, __resolution);
    ledcAttachPin(__pin_out, __channel);
    API_Resistor::set_pwm(__actual_set_pwm_percent);

    pinMode(__pin_analog_in, INPUT);
    API_Resistor::get_heat();    
}

void API_Resistor::set_pwm(int percent){
    ledcWrite(__channel, 255*(percent/100.0));
    __actual_set_pwm_percent = percent;
}

float API_Resistor::get_heat(){
    // conversion at 12bit
    float read_voltage = (analogRead(__pin_analog_in)*3.3)/4095;
    // conversion to real voltage
    float real_voltage = read_voltage*READ_TO_REAL_VOLTAGE;         
    // calc heat as Vreal^2/R
    __last_calc_heat = 1.0* (real_voltage*real_voltage/RESISTOR_VALUE);

    return __last_calc_heat;
}

