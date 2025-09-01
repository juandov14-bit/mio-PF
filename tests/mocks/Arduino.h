// Minimal Arduino mocks for desktop testing
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>
#include <iostream>
#include <sstream>

using std::uint8_t;

#ifndef HIGH
#define HIGH 1
#endif
#ifndef LOW
#define LOW 0
#endif

#ifndef INPUT
#define INPUT 0
#endif
#ifndef OUTPUT
#define OUTPUT 1
#endif

#ifndef DEC
#define DEC 10
#endif
#ifndef HEX
#define HEX 16
#endif

// Timing
inline unsigned long __mock_millis_now = 0;

inline unsigned long millis() {
  return __mock_millis_now;
}

inline void __mock_set_millis(unsigned long v) { __mock_millis_now = v; }

inline void delay(unsigned long ms) {
  __mock_millis_now += ms;
}

// GPIO / PWM stubs
inline void pinMode(int, int) {}
inline void ledcSetup(int, int, int) {}
inline void ledcAttachPin(int, int) {}
inline void ledcWrite(int, int) {}

// analogRead mockable callback
using AnalogReadCallback = int(*)(int);
inline AnalogReadCallback __mock_analog_cb = nullptr;

inline void __mock_set_analog_cb(AnalogReadCallback cb) { __mock_analog_cb = cb; }

inline int analogRead(int pin) {
  if (__mock_analog_cb) return __mock_analog_cb(pin);
  return 0;
}

// Mock Serial
class MockSerial {
public:
  void begin(unsigned long) {}
  template<typename T>
  void print(const T& v) { std::cout << v; }
  template<typename T>
  void println(const T& v) { std::cout << v << std::endl; }
  void println() { std::cout << std::endl; }
  int available() { return 0; }
  // Simple stub; not used in our tests
  const char* readStringUntil(char) { return ""; }

  // Overloads with base (DEC/HEX)
  void print(long v, int base) { print_number(v, base); }
  void print(unsigned long v, int base) { print_number(v, base); }
  void print(int v, int base) { print_number(static_cast<long>(v), base); }
  void print(uint8_t v, int base) { print_number(static_cast<unsigned long>(v), base); }

private:
  template<typename T>
  void print_number(T v, int base) {
    if (base == HEX) {
      std::ostringstream oss;
      oss << std::hex << std::uppercase << static_cast<unsigned long>(v);
      std::cout << oss.str();
    } else {
      std::cout << static_cast<long>(v);
    }
  }
};

static MockSerial Serial;

// ESP mock
class ESPClass {
public:
  void restart() { std::cerr << "[ESP.restart]\n"; }
};

static ESPClass ESP;
