/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never run).
 */
#include "my_port.hpp"
#include "xmpl100.hpp"
#include "xmpl100_kconfig.hpp"

// --8<-- [start:custom]
xmpl100::Error StartFast(xmpl100::XMPL100<MyPort>& sensor) {
  xmpl100::Config c{}; // datasheet reset values
  c.odr = xmpl100::OutputDataRate::Hz16;
  c.averaging = xmpl100::Averaging::X4; // X8 is too slow for 16 Hz: InvalidArgument
  c.offset_c = -0.25F;                  // board self-heating, measured
  if (xmpl100::Validate(c) != xmpl100::Error::Ok) {
    return xmpl100::Error::InvalidArgument; // checked without touching the chip
  }
  return sensor.Initialize(c).error;
}
// --8<-- [end:custom]

// --8<-- [start:kconfig]
xmpl100::Error StartFromMenuconfig(xmpl100::XMPL100<MyPort>& sensor) {
  return sensor.Initialize(xmpl100::KconfigDefaults()).error;
}
// --8<-- [end:kconfig]
