/**
 * @file main.cpp
 * @brief The XMPL100 driver on your PC: initialise a simulated chip and read the temperature.
 *
 * No hardware and no SDK: the host port (host_xmpl100_port.hpp) connects the real driver to the
 * register model the host tests use. The same calls run on an MCU with that MCU's port
 * (examples/esp32/ for ESP-IDF). Exit code 0 when every call succeeded.
 */
#include <cstdint>
#include <cstdio>

#include "host_xmpl100_port.hpp"
#include "xmpl100.hpp"

int main() {
  HostXmpl100Port port;
  xmpl100::XMPL100<HostXmpl100Port> sensor(port); // no I/O yet

  std::printf("XMPL100 driver %s on the host\n", xmpl100::GetDriverVersion());
  if (const auto r = sensor.Initialize(); !r) {
    std::printf("Initialize failed: %s\n", xmpl100::ToString(r.error));
    return 1;
  }

  for (int i = 0; i < 3; ++i) {
    port.DelayUs(250'000); // one conversion period at the default 4 Hz
    const auto t = sensor.ReadTemperatureC();
    if (!t) {
      std::printf("read failed: %s\n", xmpl100::ToString(t.error));
      return 1;
    }
    std::printf("%.2f C\n", static_cast<double>(t.value));
  }
  return 0;
}
