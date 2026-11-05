/**
 * @file api_tasks.cpp
 * @brief The API guide's examples, compiled by the host build (never run).
 */
#include <cstdio>

#include "my_port.hpp"
#include "xmpl100.hpp"

using Sensor = xmpl100::XMPL100<MyPort>;

// --8<-- [start:lifecycle]
xmpl100::Error BringUp(Sensor& sensor) {
  if (sensor.IsInitialized()) {
    return xmpl100::Error::Ok;
  }
  const auto r = sensor.Initialize(xmpl100::LowPowerPreset());
  if (!r) {
    std::printf("XMPL100 init: %s\n", xmpl100::ToString(r.error));
  }
  return r.error;
}
// --8<-- [end:lifecycle]

// --8<-- [start:one-shot]
xmpl100::Result<float> SampleOnDemand(Sensor& sensor) {
  if (auto r = sensor.SetMode(xmpl100::Mode::OneShot); !r) {
    return xmpl100::Result<float>::Failure(r.error);
  }
  return sensor.MeasureOnceC(); // trigger, wait for DRDY, read
}
// --8<-- [end:one-shot]

// --8<-- [start:alerts]
xmpl100::Error ArmOverTemperature(Sensor& sensor) {
  xmpl100::Config c = sensor.GetConfig();
  c.alert_enable = true;
  c.alert_polarity = xmpl100::AlertPolarity::ActiveLow; // open-drain to an MCU interrupt
  c.alert_low_c = -20.0F;
  c.alert_high_c = 85.0F;
  return sensor.ApplyConfig(c).error; // InvalidArgument if low >= high
}
// --8<-- [end:alerts]

// --8<-- [start:errors]
const char* Describe(xmpl100::Error e) {
  switch (e) {
  case xmpl100::Error::Bus:
  case xmpl100::Error::DeviceIdMismatch:
    return "check the bus wiring and the chip select or address";
  case xmpl100::Error::DeviceFault:
    return "sensing element open or shorted: replace the part";
  case xmpl100::Error::Crc:
    return "signal integrity: lower the bus clock";
  default:
    return xmpl100::ToString(e);
  }
}
// --8<-- [end:errors]

// --8<-- [start:deinit]
void PowerDown(Sensor& sensor) {
  if (auto r = sensor.Deinitialize(); !r) {
    std::printf("XMPL100 shutdown write failed: %s\n", xmpl100::ToString(r.error));
  }
}
// --8<-- [end:deinit]
