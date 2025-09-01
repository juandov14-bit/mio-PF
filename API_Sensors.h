#ifndef API_Sensors_h
#define API_Sensors_h
 
#include "Arduino.h"
#include "OneWireNg_CurrentPlatform.h"
#include <DallasTemperature.h>


#define ONE_WIRE_BUS    15
#define DEVICES_CONNECT 5

class API_Sensors {
public:
    API_Sensors();
    void init(bool print_init = false);
    void getTemperatures(float write_data[DEVICES_CONNECT]);
    float getTemperatureId(uint8_t id_sensor = 1);
    
private:    
    OneWire* __oneWire;
    DallasTemperature* __sensors;
    DeviceAddress __tempDeviceAddress;
    int __numberOfDevices;
    
    float __temperature_data[4];


    void printAddress(DeviceAddress deviceAddress);    
};
 
#endif
