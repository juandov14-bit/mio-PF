#include "API_MyTimer.h"

API_MyTimer::API_MyTimer() {
  API_MyTimer::restart();
}

void API_MyTimer::upd_time() {
  if( (millis()-__previous_millis)>=1000 ) {
    __previous_millis = millis();
    __seconds++;
    if( __seconds==60 ){
      __seconds = 0;
      __minutes++;
    }    
  }  
}

float API_MyTimer::get_minutes() {
  API_MyTimer::upd_time();  
  return (__minutes + __seconds/60.0f);
}

void API_MyTimer::restart() {
  __previous_millis = millis();
  __seconds = 0;
  __minutes = 0;
}

