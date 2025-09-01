// Simple tests for the project using desktop mocks
#include <cassert>
#include <cmath>
#include <iostream>

// Include project headers (will use mocked Arduino + libs)
#include "API_Control_PID.h"
#include "API_MyTimer.h"
#include "API_Resistor.h"
#include "API_Sensors.h"

// Mocks
#include "tests/mocks/Arduino.h"
#include "tests/mocks/DallasTemperature.h"

static void test_pid_basic() {
  PID_config cfg = {0,0,30.0f, 0.5f, 0.1f, 1.0f, 1.0f, 0.0f, 2.0f, 0.0f};
  API_Control_PID pid;
  pid.configure(cfg);

  float y = 20.0f; // measured
  float u = pid.update(y);
  // u = KP*(Kb*(ref) - y) + t_integ = 0.5*(1*30-20)+0 = 5 -> clamp to 2.0
  assert(std::abs(u - 2.0f) < 1e-5);
}

static void test_timer_minutes() {
  API_MyTimer t;
  __mock_set_millis(0);
  t.restart();
  // advance 90 seconds
  for (int i=0;i<90;i++) { __mock_set_millis(millis()+1000); t.get_minutes(); }
  float m = t.get_minutes();
  std::cout << "timer minutes: " << m << std::endl;
  // 1.5 minutes expected
  assert(std::abs(m - 1.5f) < 0.05f);
}

static int fake_adc_half_scale(int) { return 2048; }

static void test_resistor_heat_calc() {
  __mock_set_analog_cb(fake_adc_half_scale);
  API_Resistor r;
  // Expect: read_voltage = 2048*3.3/4095 ≈ 1.6504
  // real_voltage = read*READ_TO_REAL_VOLTAGE
  // heat = V^2 / R
  float read_v = (2048.0f*3.3f)/4095.0f;
  float real_v = read_v * READ_TO_REAL_VOLTAGE;
  float expected = (real_v*real_v)/RESISTOR_VALUE;
  float h = r.get_heat();
  assert(std::abs(h - expected) < 1e-3f);
}

static void test_sensors_read() {
  DallasTemperature::__mock_set_devices(DEVICES_CONNECT); // avoid restart
  DallasTemperature::__mock_set_base_temp(23.0f);
  API_Sensors s;
  float v[DEVICES_CONNECT] = {0};
  s.getTemperatures(v);
  // With base 23 and +i%5, first 5 calls produce values >= 23
  for (int i=0;i<DEVICES_CONNECT;i++) {
    assert(v[i] >= 23.0f);
  }
}

int main() {
  std::cout << "Running tests...\n";
  test_pid_basic();
  test_timer_minutes();
  test_resistor_heat_calc();
  test_sensors_read();
  std::cout << "All tests passed.\n";
  return 0;
}
