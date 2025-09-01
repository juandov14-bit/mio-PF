#ifndef API_MyTimer_h
#define API_MyTimer_h
 
#include "Arduino.h"

class API_MyTimer {
public:
    API_MyTimer();
    void restart();
    float get_minutes();
  
private:
  void upd_time();

  long __previous_millis;
  int __seconds = 0;
  int __minutes = 0;  
};
 
#endif
