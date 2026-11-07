/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_port.hpp"

// --8<-- [start:quickstart]
#include "xmpl100.hpp"

int read_board_temperature(MyPort& port, float& celsius) {
  xmpl100::XMPL100<MyPort> sensor(port); // no I/O yet

  if (auto r = sensor.EnsureInitialized(); !r) { // 1. bus up, reset, CHIP_ID, Config{}
    return static_cast<int>(r.error); //    Bus, Timeout or DeviceIdMismatch: check wiring
  }
  const auto t = sensor.ReadTemperatureC(); // 2. latest continuous result
  if (!t) {
    return static_cast<int>(t.error); //    DeviceFault: sensing element open or shorted
  }
  celsius = t.value; // 3. degrees Celsius, 1/128 °C resolution
  return 0;
}
// --8<-- [end:quickstart]
