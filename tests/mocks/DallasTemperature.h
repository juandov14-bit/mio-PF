// Minimal DallasTemperature mock
#pragma once

#include <cstdint>
#include "OneWire.h"

using std::uint8_t;

typedef uint8_t DeviceAddress[8];

class DallasTemperature {
public:
  explicit DallasTemperature(OneWire*) {}

  void begin() {}

  int getDeviceCount() const { return devices_; }

  bool getAddress(DeviceAddress& addr, int i) {
    (void)addr; (void)i; return true;
  }

  void requestTemperatures() {}

  float getTempC(DeviceAddress) {
    // Return a deterministic temperature in tests
    float t = base_temp_ + static_cast<float>(counter_ % 5);
    counter_++;
    return t;
  }

  // Test controls
  static void __mock_set_devices(int n) { devices_ = n; }
  static void __mock_set_base_temp(float t) { base_temp_ = t; }

private:
  static inline int devices_ = 5;
  static inline float base_temp_ = 25.0f;
  static inline int counter_ = 0;
};

