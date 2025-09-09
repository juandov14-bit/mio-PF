#include "API_Resistor.h"
#include <driver/adc.h>
#include <esp_log.h>

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

static adc1_channel_t mapPinToAdc1Channel(int pin) {
    switch (pin) {
        case 36: return ADC1_CHANNEL_0; // VP
        case 37: return ADC1_CHANNEL_1; // VN
        case 38: return ADC1_CHANNEL_2;
        case 39: return ADC1_CHANNEL_3;
        case 32: return ADC1_CHANNEL_4;
        case 33: return ADC1_CHANNEL_5;
        case 34: return ADC1_CHANNEL_6;
        case 35: return ADC1_CHANNEL_7;
        default: return ADC1_CHANNEL_4; // fallback GPIO32
    }
}

void API_Resistor::init(){
    ledcSetup(__channel, __freq, __resolution);
    ledcAttachPin(__pin_out, __channel);
    API_Resistor::set_pwm(__actual_set_pwm_percent);

    // Silenciar logs del driver ADC si fueran ruidosos
    esp_log_level_set("ADC", ESP_LOG_NONE);

    // Configuración del ADC1 vía driver IDF (evita problemas de lock)
    adc1_channel_t ch = mapPinToAdc1Channel(__pin_analog_in);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ch, ADC_ATTEN_DB_12);

    API_Resistor::get_heat();
}

void API_Resistor::set_pwm(int percent){
    ledcWrite(__channel, 255*(percent/100.0));
    __actual_set_pwm_percent = percent;
}

float API_Resistor::get_heat(){
    // Lectura mediante driver IDF (ADC1)
    adc1_channel_t ch = mapPinToAdc1Channel(__pin_analog_in);
    int raw = adc1_get_raw(ch);
    float read_voltage = (raw * 3.3f) / 4095.0f;
    // Conversión a tensión real de la resistencia
    float real_voltage = read_voltage * READ_TO_REAL_VOLTAGE;
    // Potencia: V^2/R
    __last_calc_heat = (real_voltage * real_voltage) / RESISTOR_VALUE;
    return __last_calc_heat;
}
