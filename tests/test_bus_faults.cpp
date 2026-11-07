/**
 * @file test_bus_faults.cpp
 * @brief Every public operation reports a failed bus transfer as Error::Bus, whichever transfer
 * fails.
 *
 * Each operation is run once on a clean bus to count its transfers, then once per transfer with
 * exactly that transfer failing. This reaches every "the transfer failed, return the error" path in
 * the driver, which a single fault test per operation would not.
 */
#include <cstdio>

#include "mock_xmpl100.hpp"
#include "test_harness.hpp"
#include "xmpl100.hpp"

using xmpl100::Error;
using xmpl100_test::FullPort;
using xmpl100_test::MinimalPort;

namespace {

/// Runs `op` with each of its transfers failing in turn; every run must return Error::Bus.
template <typename Port, typename Op>
void SweepBusFaults(const char* name, const xmpl100::Config& config, Op op) {
  int transfers = 0;
  {
    Port clean;
    xmpl100::XMPL100<Port> sensor(clean, config);
    REQUIRE(sensor.EnsureInitialized().ok());
    const int before = clean.state.transfers;
    CHECK_EQ(op(sensor), Error::Ok);
    transfers = clean.state.transfers - before;
  }
  CHECK(transfers > 0);
  for (int i = 0; i < transfers; ++i) {
    Port port;
    xmpl100::XMPL100<Port> sensor(port, config);
    REQUIRE(sensor.EnsureInitialized().ok());
    port.state.fail_transfer = port.state.transfers + i;
    const Error e = op(sensor);
    if (e != Error::Bus) {
      std::printf("    %s: transfer %d of %d failed, got %s\n", name, i + 1, transfers,
                  xmpl100::ToString(e));
    }
    CHECK_EQ(e, Error::Bus);
  }
}

xmpl100::Config Shutdown() {
  xmpl100::Config c{};
  c.mode = xmpl100::Mode::Shutdown;
  return c;
}

xmpl100::Config WithCrc() {
  xmpl100::Config c{};
  c.crc_enable = true;
  return c;
}

} // namespace

TEST_CASE("Every transfer of every read can fail and is reported as Bus") {
  using S = xmpl100::XMPL100<MinimalPort>;
  SweepBusFaults<MinimalPort>("ReadStatus", {}, [](S& s) { return s.ReadStatus().error; });
  SweepBusFaults<MinimalPort>("ReadRawTemperature", {},
                              [](S& s) { return s.ReadRawTemperature().error; });
  SweepBusFaults<MinimalPort>("ReadTemperatureC", {},
                              [](S& s) { return s.ReadTemperatureC().error; });
  SweepBusFaults<MinimalPort>("ReadTemperatureC with CRC", WithCrc(),
                              [](S& s) { return s.ReadTemperatureC().error; });
}

TEST_CASE("Every transfer of every configuration change can fail and is reported as Bus") {
  using S = xmpl100::XMPL100<MinimalPort>;
  SweepBusFaults<MinimalPort>("ApplyConfig", {}, [](S& s) { return s.ApplyConfig({}).error; });
  SweepBusFaults<MinimalPort>("SetMode", {},
                              [](S& s) { return s.SetMode(xmpl100::Mode::Shutdown).error; });
  SweepBusFaults<MinimalPort>("SetAlertThresholdsC", {},
                              [](S& s) { return s.SetAlertThresholdsC(10.0F, 40.0F).error; });
  SweepBusFaults<MinimalPort>("SoftReset", {}, [](S& s) { return s.SoftReset().error; });
  SweepBusFaults<MinimalPort>("Deinitialize", {}, [](S& s) { return s.Deinitialize().error; });
}

TEST_CASE("Every transfer of a one-shot measurement can fail and is reported as Bus") {
  using S = xmpl100::XMPL100<MinimalPort>;
  SweepBusFaults<MinimalPort>("TriggerOneShot", Shutdown(),
                              [](S& s) { return s.TriggerOneShot().error; });
  SweepBusFaults<MinimalPort>("MeasureOnceC", Shutdown(),
                              [](S& s) { return s.MeasureOnceC().error; });
  SweepBusFaults<MinimalPort>("WaitDataReady", {}, [](S& s) { return s.WaitDataReady().error; });
}

TEST_CASE("Every transfer of a hardware reset can fail and is reported as Bus") {
  using S = xmpl100::XMPL100<FullPort>;
  SweepBusFaults<FullPort>("HardwareReset", {}, [](S& s) { return s.HardwareReset().error; });
}
