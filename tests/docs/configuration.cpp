/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never run).
 */
#include "my_port.hpp"
#include "xmpl100.hpp"
#include "xmpl100_kconfig.hpp"

// --8<-- [start:custom]
xmpl100::Config FastConfig() {
  xmpl100::Config c{}; // datasheet reset values
  c.odr = xmpl100::OutputDataRate::Hz16;
  c.averaging = xmpl100::Averaging::X4; // X8 is too slow for 16 Hz: InvalidArgument
  c.offset_c = -0.25F;                  // board self-heating, measured
  return c;                             // xmpl100::Validate(c) checks it without the chip
}

xmpl100::Error StartFast(MyPort& port) {
  static xmpl100::XMPL100<MyPort> sensor(port, FastConfig()); // stored, no I/O
  return sensor.EnsureInitialized().error; // InvalidArgument if the config is out of range
}
// --8<-- [end:custom]

// --8<-- [start:kconfig]
xmpl100::Error StartFromMenuconfig(MyPort& port) {
  static xmpl100::XMPL100<MyPort> sensor(port, xmpl100::KconfigDefaults());
  return sensor.EnsureInitialized().error;
}
// --8<-- [end:kconfig]
