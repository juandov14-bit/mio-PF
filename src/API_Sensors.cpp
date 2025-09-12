#include "API_Sensors.h"


API_Sensors::API_Sensors() {
    __oneWire = new OneWire(ONE_WIRE_BUS);
    __sensors = new DallasTemperature(__oneWire);
    DeviceAddress __tempDeviceAddress;
    // Diferir init hasta después de Serial.begin() en setup()
}

void API_Sensors::init(bool print_init){
  // Start up the library
  __sensors->begin();

  // Grab a count of devices on the wire
  __numberOfDevices = __sensors->getDeviceCount();

  // Si el conteo no coincide, informar pero no reiniciar aquí
  if(__numberOfDevices!=DEVICES_CONNECT){
    Serial.print("[Sensors] Discrepancia de cantidad. Esperados: ");
    Serial.print(DEVICES_CONNECT);
    Serial.print(" detectados: ");
    Serial.println(__numberOfDevices);
  }
  
  if(print_init){
  // locate devices on the bus
  Serial.print("Locating devices...");
  Serial.print("Found ");
  Serial.print(__numberOfDevices, DEC);
  Serial.println(" devices.");  
    
  // Loop through each device, print out address
  for(int i=0;i<__numberOfDevices; i++){
    // Search the wire for address
    if(__sensors->getAddress(__tempDeviceAddress, i)){
      Serial.print("Found device ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      API_Sensors::printAddress(__tempDeviceAddress);
      Serial.println();
      } else {
      Serial.print("Found ghost device at ");
      Serial.print(i, DEC);
      Serial.print(" but could not detect address. Check power and cabling");
      }
    }    
  }

}


void API_Sensors::getTemperatures(float write_data[DEVICES_CONNECT]){
  float tempC;
  Serial.println("[Sensors] getTemperatures() called");
  __sensors->requestTemperatures(); // Send the command to get temperatures

  // Loop through each device, print out temperature data
  for (int i = 0; i < DEVICES_CONNECT; i++) {
    // Search the wire for address
    if (__sensors->getAddress(__tempDeviceAddress, i)) {
      tempC = __sensors->getTempC(__tempDeviceAddress);
      Serial.print("[Sensors] idx "); Serial.print(i);
      Serial.print(" addr="); API_Sensors::printAddress(__tempDeviceAddress);
      Serial.print(" temp="); Serial.println(tempC);
      if (tempC>5) { __temperature_data[i] = tempC; }
    } else {
      Serial.print("[Sensors] idx "); Serial.print(i);
      Serial.println(" no address detected");
    }
    // Always reflect current cached value to output buffer
    write_data[i] = __temperature_data[i];
  }
}

float API_Sensors::getTemperatureId(uint8_t id_sensor) {
  float write_data[DEVICES_CONNECT];
  API_Sensors::getTemperatures(write_data);  
  return write_data[id_sensor];
}


void API_Sensors::printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
  }
}
