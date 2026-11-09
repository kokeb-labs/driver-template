/**
 * @file test_xmpl100.cpp
 * @brief Driver behaviour over the register model: lifecycle, configuration, measurement, port
 * hooks.
 */
#include "mock_xmpl100.hpp"
#include "test_harness.hpp"
#include "xmpl100.hpp"

using xmpl100::Error;
using xmpl100_test::FullPort;
using xmpl100_test::MinimalPort;
namespace reg = xmpl100::reg;

// --- Lifecycle
// --------------------------------------------------------------------------------------------------

TEST_CASE("The constructor does no I/O") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  CHECK(!sensor.IsInitialized());
  CHECK_EQ(port.state.transfers, 0);
  CHECK_EQ(port.state.bus_up_calls, 0);
}

TEST_CASE("Every I/O call returns NotInitialized before EnsureInitialized, and does no I/O") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  CHECK_EQ(sensor.ReadStatus().error, Error::NotInitialized);
  CHECK_EQ(sensor.ReadRawTemperature().error, Error::NotInitialized);
  CHECK_EQ(sensor.ReadTemperatureC().error, Error::NotInitialized);
  CHECK_EQ(sensor.TriggerOneShot().error, Error::NotInitialized);
  CHECK_EQ(sensor.WaitDataReady().error, Error::NotInitialized);
  CHECK_EQ(sensor.MeasureOnceC().error, Error::NotInitialized);
  CHECK_EQ(sensor.SoftReset().error, Error::NotInitialized);
  CHECK_EQ(sensor.ReadRegister(reg::kChipId).error, Error::NotInitialized);
  CHECK_EQ(port.state.transfers, 0);
  CHECK_EQ(port.state.bus_up_calls, 0); // no call starts the chip as a side effect
  CHECK(!sensor.IsInitialized());
}

TEST_CASE("HardwareReset returns NotInitialized before EnsureInitialized") {
  FullPort port;
  xmpl100::XMPL100<FullPort> sensor(port);
  CHECK_EQ(sensor.HardwareReset().error, Error::NotInitialized);
  CHECK_EQ(port.state.transfers, 0);
}

TEST_CASE("Without GpioSet, EnsureInitialized soft-resets and HardwareReset is Unsupported") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  const int resets = port.state.chip.resets;
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(port.state.chip.resets, resets + 1); // the soft reset
  CHECK_EQ(sensor.HardwareReset().error, Error::Unsupported);
  CHECK(sensor.IsInitialized()); // nothing was touched
}

TEST_CASE("With GpioSet, EnsureInitialized pulses RESET_N instead of the soft reset") {
  FullPort port;
  xmpl100::XMPL100<FullPort> sensor(port);
  const int resets = port.state.chip.resets;
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(port.reset_pulses, 1);
  CHECK_EQ(port.state.chip.resets, resets + 1); // the pin reset only
  CHECK(!port.reset_asserted);
}

TEST_CASE("EnsureInitialized resets the chip, checks CHIP_ID and reads the revision") {
  FullPort port;
  xmpl100::XMPL100<FullPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK(sensor.IsInitialized());
  CHECK_EQ(sensor.GetRevision(), xmpl100::kRevisionB);
  CHECK(port.delayed_us >= xmpl100::kResetTimeMaxUs); // waited t_RST with the delay hook
  CHECK_EQ(port.state.chip.Register(reg::kCtrl), reg::kCtrlReset);
  CHECK_EQ(port.state.chip.Register(reg::kConfig), reg::kConfigReset);
}

TEST_CASE("Config{} writes the reset values back") {
  FullPort port;
  xmpl100::XMPL100<FullPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(port.state.chip.Register(reg::kAlertHighMsb), reg::kAlertHighReset >> 8);
  CHECK_EQ(port.state.chip.Register(reg::kAlertHighMsb + 1), reg::kAlertHighReset & 0xFF);
  CHECK_EQ(port.state.chip.Register(reg::kAlertLowMsb), reg::kAlertLowReset >> 8);
  CHECK_EQ(port.state.chip.Register(reg::kAlertLowMsb + 1), reg::kAlertLowReset & 0xFF);
  CHECK_EQ(port.state.chip.Register(reg::kOffset), 0);
}

TEST_CASE("EnsureInitialized starts the chip once, then returns Ok with no I/O") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  const int transfers = port.state.transfers;
  const int bus_up_calls = port.state.bus_up_calls;
  const int resets = port.state.chip.resets;
  for (int i = 0; i < 3; ++i) {
    CHECK(sensor.EnsureInitialized().ok());
  }
  CHECK_EQ(port.state.transfers, transfers);
  CHECK_EQ(port.state.bus_up_calls, bus_up_calls);
  CHECK_EQ(port.state.chip.resets, resets);
}

TEST_CASE("After a failed start, EnsureInitialized starts again from the beginning") {
  MinimalPort port;
  port.state.chip.chip_id = 0x5A;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  CHECK_EQ(sensor.EnsureInitialized().error, Error::DeviceIdMismatch);
  CHECK(!sensor.IsInitialized());
  const int transfers = port.state.transfers;
  CHECK_EQ(sensor.ReadTemperatureC().error, Error::NotInitialized);
  CHECK_EQ(port.state.transfers, transfers);  // still no implicit start
  port.state.chip.chip_id = xmpl100::kChipId; // the right chip answers now
  const int resets = port.state.chip.resets;
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(port.state.chip.resets, resets + 1); // reset again, not resumed
  CHECK(sensor.IsInitialized());
}

TEST_CASE("Deinitialize then EnsureInitialized restarts the chip") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port, xmpl100::LowPowerPreset());
  REQUIRE(sensor.EnsureInitialized().ok());
  REQUIRE(sensor.Deinitialize().ok());
  const int transfers = port.state.transfers;
  CHECK_EQ(sensor.ReadStatus().error, Error::NotInitialized);
  CHECK_EQ(port.state.transfers, transfers);
  const int resets = port.state.chip.resets;
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(port.state.chip.resets, resets + 1);
  CHECK_EQ(port.state.chip.Register(reg::kConfig) & reg::kConfigOdrMask, 0); // 1 Hz again
  CHECK(sensor.ReadTemperatureC().ok());
}

TEST_CASE("ApplyConfig and its setters before the start store the config with no I/O") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  xmpl100::Config bad{};
  bad.conversion_timeout_ms = 0;
  CHECK_EQ(sensor.ApplyConfig(bad).error, Error::InvalidArgument); // checked, not stored
  CHECK_EQ(sensor.GetConfig().conversion_timeout_ms, xmpl100::Config{}.conversion_timeout_ms);
  REQUIRE(sensor.ApplyConfig(xmpl100::LowPowerPreset()).ok());
  REQUIRE(sensor.SetMode(xmpl100::Mode::Shutdown).ok());
  REQUIRE(sensor.SetAlertThresholdsC(0.0F, 50.0F).ok());
  CHECK_EQ(sensor.SetAlertThresholdsC(30.0F, 20.0F).error, Error::InvalidArgument);
  CHECK_EQ(port.state.transfers, 0);
  CHECK_EQ(port.state.bus_up_calls, 0);
  CHECK(!sensor.IsInitialized());
  CHECK_EQ(sensor.GetConfig().mode, xmpl100::Mode::Shutdown);
  REQUIRE(sensor.EnsureInitialized().ok()); // applies what was stored
  CHECK_EQ(port.state.chip.Register(reg::kCtrl) & reg::kCtrlModeMask, 0);
  CHECK_EQ(port.state.chip.Register(reg::kConfig) & reg::kConfigOdrMask, 0); // 1 Hz
  CHECK_EQ(port.state.chip.Register(reg::kAlertHighMsb), (50U * 128U) >> 8U);
}

TEST_CASE("A port that cannot bring the bus up fails with Bus and logs it") {
  FullPort port;
  port.state.bus_up_ok = false;
  xmpl100::XMPL100<FullPort> sensor(port);
  CHECK_EQ(sensor.EnsureInitialized().error, Error::Bus);
  CHECK(!sensor.IsInitialized());
  CHECK_EQ(port.error_lines, 1);
}

TEST_CASE("A wrong CHIP_ID fails with DeviceIdMismatch") {
  MinimalPort port;
  port.state.chip.chip_id = 0x5A;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  CHECK_EQ(sensor.EnsureInitialized().error, Error::DeviceIdMismatch);
  CHECK(!sensor.IsInitialized());
}

TEST_CASE("A reset that never finishes fails with Timeout, soft or pin reset") {
  MinimalPort minimal;
  minimal.state.chip.reset_stuck = true;
  xmpl100::XMPL100<MinimalPort> a(minimal);
  CHECK_EQ(a.EnsureInitialized().error, Error::Timeout);

  FullPort full;
  full.state.chip.reset_stuck = true;
  xmpl100::XMPL100<FullPort> b(full);
  CHECK_EQ(b.EnsureInitialized().error, Error::Timeout);
}

TEST_CASE("Every failed transfer during EnsureInitialized is reported as Bus") {
  int transfers = 0;
  {
    MinimalPort clean;
    xmpl100::XMPL100<MinimalPort> sensor(clean);
    REQUIRE(sensor.EnsureInitialized().ok());
    transfers = clean.state.transfers;
  }
  CHECK(transfers >= 8);
  for (int i = 0; i < transfers; ++i) {
    MinimalPort port;
    port.state.fail_transfer = i;
    xmpl100::XMPL100<MinimalPort> sensor(port);
    const auto r = sensor.EnsureInitialized();
    CHECK_EQ(r.error, Error::Bus);
    CHECK(!sensor.IsInitialized());
  }
}

TEST_CASE("An invalid Config is rejected before any I/O") {
  MinimalPort port;
  xmpl100::Config c{};
  c.alert_low_c = 50.0F;
  c.alert_high_c = 40.0F;
  xmpl100::XMPL100<MinimalPort> sensor(port, c);
  CHECK_EQ(sensor.EnsureInitialized().error, Error::InvalidArgument);
  CHECK_EQ(port.state.transfers, 0);
}

TEST_CASE("Deinitialize puts the chip in shutdown and is idempotent") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  CHECK(sensor.Deinitialize().ok()); // not initialised: nothing to do
  REQUIRE(sensor.EnsureInitialized().ok());
  REQUIRE(sensor.Deinitialize().ok());
  CHECK(!sensor.IsInitialized());
  CHECK_EQ(port.state.chip.Register(reg::kCtrl) & reg::kCtrlModeMask, 0);
  CHECK_EQ(sensor.ReadTemperatureC().error, Error::NotInitialized);
}

TEST_CASE("SoftReset and HardwareReset reset the chip and re-apply the configuration") {
  FullPort port;
  xmpl100::XMPL100<FullPort> sensor(port, xmpl100::LowPowerPreset());
  REQUIRE(sensor.EnsureInitialized().ok());
  const int resets = port.state.chip.resets;
  REQUIRE(sensor.SoftReset().ok());
  CHECK_EQ(port.state.chip.resets, resets + 1);
  CHECK_EQ(port.state.chip.Register(reg::kConfig) & reg::kConfigOdrMask, 0); // 1 Hz again
  REQUIRE(sensor.HardwareReset().ok());
  CHECK_EQ(port.state.chip.resets, resets + 2);
  CHECK(!port.reset_asserted);
  CHECK_EQ(port.state.chip.Register(reg::kConfig) & reg::kConfigOdrMask, 0);
}

TEST_CASE("A failed RESET_N write fails HardwareReset with Bus") {
  FullPort port;
  xmpl100::XMPL100<FullPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  port.gpio_ok = false;
  CHECK_EQ(sensor.HardwareReset().error, Error::Bus);
  CHECK(!sensor.IsInitialized());
}

// --- Configuration
// ----------------------------------------------------------------------------------------------

TEST_CASE("ApplyConfig writes CTRL, CONFIG, the thresholds and the offset") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  xmpl100::Config c = xmpl100::HighAccuracyPreset();
  c.alert_enable = true;
  c.alert_polarity = xmpl100::AlertPolarity::ActiveHigh;
  c.alert_low_c = -10.0F;
  c.alert_high_c = 60.5F;
  c.offset_c = -0.5F;
  REQUIRE(sensor.ApplyConfig(c).ok());
  const auto& chip = port.state.chip;
  CHECK_EQ(chip.Register(reg::kCtrl), 0x02U | reg::kCtrlAlertEn | reg::kCtrlAlertPol);
  CHECK_EQ(chip.Register(reg::kConfig),
           0x03U | (0x03U << reg::kConfigAvgShift) | reg::kConfigCrcEn);
  const uint32_t high = (static_cast<uint32_t>(chip.Register(reg::kAlertHighMsb)) << 8U) |
                        chip.Register(reg::kAlertHighMsb + 1);
  CHECK_EQ(high, 7744U); // 60.5 °C * 128
  const auto low =
      static_cast<int16_t>((static_cast<uint32_t>(chip.Register(reg::kAlertLowMsb)) << 8U) |
                           chip.Register(reg::kAlertLowMsb + 1));
  CHECK_EQ(low, -1280);
  CHECK_EQ(static_cast<int8_t>(chip.Register(reg::kOffset)), -8);
  CHECK(sensor.GetConfig().crc_enable);
}

TEST_CASE("Validate rejects every out-of-range field") {
  CHECK_EQ(xmpl100::Validate({}), Error::Ok);
  CHECK_EQ(xmpl100::Validate(xmpl100::LowPowerPreset()), Error::Ok);
  CHECK_EQ(xmpl100::Validate(xmpl100::HighAccuracyPreset()), Error::Ok);
  CHECK_EQ(xmpl100::Validate(xmpl100::OneShotPreset()), Error::Ok);
  xmpl100::Config c{};
  c.mode = static_cast<xmpl100::Mode>(3);
  CHECK_EQ(xmpl100::Validate(c), Error::InvalidArgument);
  c = {};
  c.odr = static_cast<xmpl100::OutputDataRate>(5);
  CHECK_EQ(xmpl100::Validate(c), Error::InvalidArgument);
  c = {};
  c.alert_high_c = 151.0F;
  CHECK_EQ(xmpl100::Validate(c), Error::InvalidArgument);
  c = {};
  c.offset_c = 8.0F;
  CHECK_EQ(xmpl100::Validate(c), Error::InvalidArgument);
  c = {};
  c.conversion_timeout_ms = 0;
  CHECK_EQ(xmpl100::Validate(c), Error::InvalidArgument);
  c = {};
  c.odr = xmpl100::OutputDataRate::Hz16;
  c.averaging = xmpl100::Averaging::X8; // 80 ms per result, 62.5 ms period
  CHECK_EQ(xmpl100::Validate(c), Error::InvalidArgument);
  c.averaging = xmpl100::Averaging::X4;
  CHECK_EQ(xmpl100::Validate(c), Error::Ok);
}

TEST_CASE("SetMode and SetAlertThresholdsC change only their fields") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port, xmpl100::LowPowerPreset());
  REQUIRE(sensor.EnsureInitialized().ok());
  REQUIRE(sensor.SetMode(xmpl100::Mode::Shutdown).ok());
  CHECK_EQ(port.state.chip.Register(reg::kCtrl) & reg::kCtrlModeMask, 0);
  CHECK_EQ(port.state.chip.Register(reg::kConfig) & reg::kConfigOdrMask, 0);
  CHECK_EQ(sensor.SetAlertThresholdsC(30.0F, 20.0F).error, Error::InvalidArgument);
  REQUIRE(sensor.SetAlertThresholdsC(0.0F, 100.0F).ok());
  CHECK_EQ(sensor.GetConfig().mode, xmpl100::Mode::Shutdown);
}

TEST_CASE("CRC on revision A silicon is Unsupported") {
  MinimalPort port;
  port.state.chip.rev_id = xmpl100::kRevisionA;
  xmpl100::XMPL100<MinimalPort> sensor(port, xmpl100::HighAccuracyPreset());
  CHECK_EQ(sensor.EnsureInitialized().error, Error::Unsupported);
  REQUIRE(sensor.ApplyConfig({}).ok()); // not started: stored for the next attempt
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(sensor.ApplyConfig(xmpl100::HighAccuracyPreset()).error, Error::Unsupported);
}

// --- Measurement
// ------------------------------------------------------------------------------------------------

TEST_CASE("ReadTemperatureC converts counts and applies the offset") {
  MinimalPort port;
  port.state.chip.temperature_counts = static_cast<int16_t>(-1000); // -7.8125 °C
  xmpl100::XMPL100<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  auto t = sensor.ReadTemperatureC();
  REQUIRE(t.ok());
  CHECK(t.value == -7.8125F);
  xmpl100::Config c{};
  c.offset_c = 1.0F;
  REQUIRE(sensor.ApplyConfig(c).ok());
  auto raw = sensor.ReadRawTemperature();
  REQUIRE(raw.ok());
  CHECK_EQ(raw.value, -1000 + 128);
}

TEST_CASE("CRC-checked reads pass, and a corrupted CRC fails with Crc") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port, xmpl100::HighAccuracyPreset());
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK(sensor.ReadTemperatureC().ok());
  port.state.chip.corrupt_crc = true;
  CHECK_EQ(sensor.ReadTemperatureC().error, Error::Crc);
}

TEST_CASE("A sensor fault is reported as DeviceFault") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  port.state.chip.fault = true;
  CHECK_EQ(sensor.ReadTemperatureC().error, Error::DeviceFault);
  auto s = sensor.ReadStatus();
  REQUIRE(s.ok());
  CHECK(s.value.fault);
}

TEST_CASE("ReadStatus decodes and clears POR") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  auto s = sensor.ReadStatus();
  REQUIRE(s.ok());
  CHECK(!s.value.power_on_reset); // EnsureInitialized already read it
  CHECK(s.value.data_ready);      // continuous mode
  CHECK(!s.value.busy);
}

TEST_CASE("MeasureOnceC runs one conversion and the chip returns to shutdown") {
  FullPort port;
  port.state.chip.temperature_counts = 30 * 128;
  xmpl100::XMPL100<FullPort> sensor(port, xmpl100::OneShotPreset());
  REQUIRE(sensor.EnsureInitialized().ok());
  auto t = sensor.MeasureOnceC();
  REQUIRE(t.ok());
  CHECK(t.value == 30.0F);
  CHECK_EQ(port.state.chip.one_shots, 1);
  CHECK_EQ(port.state.chip.Register(reg::kCtrl) & reg::kCtrlModeMask, 0);
}

TEST_CASE("MeasureOnceC works on the minimal port") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port, xmpl100::OneShotPreset());
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK(sensor.MeasureOnceC().ok());
}

TEST_CASE("TriggerOneShot is Busy in continuous mode and while a conversion runs") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok()); // continuous
  CHECK_EQ(sensor.TriggerOneShot().error, Error::Busy);
  REQUIRE(sensor.SetMode(xmpl100::Mode::OneShot).ok());
  port.state.chip.conversion_busy_reads = 10;
  REQUIRE(sensor.TriggerOneShot().ok());
  CHECK_EQ(sensor.TriggerOneShot().error, Error::Busy);
}

TEST_CASE("A conversion that never finishes times out after conversion_timeout_ms") {
  FullPort port;
  port.state.chip.conversion_stuck = true;
  xmpl100::Config c = xmpl100::OneShotPreset();
  c.conversion_timeout_ms = 20;
  xmpl100::XMPL100<FullPort> sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  const uint64_t before = port.delayed_us;
  CHECK_EQ(sensor.MeasureOnceC().error, Error::Timeout);
  const uint64_t waited = port.delayed_us - before;
  CHECK(waited >= 20000);
  CHECK(waited <= 22000);
}

TEST_CASE(
    "A chip reset behind the driver's back is UnexpectedReset, and EnsureInitialized recovers") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port, xmpl100::LowPowerPreset());
  REQUIRE(sensor.EnsureInitialized().ok());
  port.state.chip.PowerOn(); // brown-out: registers back to reset values, POR set
  CHECK_EQ(sensor.ReadTemperatureC().error, Error::UnexpectedReset);
  CHECK(!sensor.IsInitialized());
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(port.state.chip.Register(reg::kConfig) & reg::kConfigOdrMask, 0); // 1 Hz again
  CHECK(sensor.ReadTemperatureC().ok());
}

// --- Diagnostics (handbook KB-CPP-119, KB-CPP-129)
// --------------------------------------------------------------------------------------------------

TEST_CASE("ReadRegister reads one register in one transfer and refuses an unknown address") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  const int transfers = port.state.transfers;
  const auto id = sensor.ReadRegister(reg::kChipId);
  REQUIRE(id.ok());
  CHECK_EQ(id.value, xmpl100::kChipId);
  CHECK_EQ(port.state.transfers, transfers + 1); // single shot, no retry
  CHECK_EQ(sensor.ReadRegister(reg::kLastRegister + 1).error, Error::InvalidArgument);
  CHECK_EQ(port.state.transfers, transfers + 1); // nothing sent
}

TEST_CASE("The start-up leaves the latch empty and GetLastStatusByte holds the last STATUS read") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  CHECK_EQ(sensor.GetLastStatusByte(), 0);
  REQUIRE(sensor.EnsureInitialized().ok()); // read STATUS, POR included: the driver's own reset
  CHECK(!sensor.GetLatchedStatus().power_on_reset);
  port.state.chip.fault = true;
  REQUIRE(sensor.ReadStatus().ok());
  CHECK((sensor.GetLastStatusByte() & reg::kStatusFault) != 0);
  const int transfers = port.state.transfers;
  (void)sensor.GetLastStatusByte(); // no I/O
  (void)sensor.GetLatchedStatus();
  CHECK_EQ(port.state.transfers, transfers);
}

TEST_CASE("POR seen by an internal STATUS read stays in the latch until ClearLatchedStatus") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port, xmpl100::LowPowerPreset());
  REQUIRE(sensor.EnsureInitialized().ok());
  port.state.chip.PowerOn(); // POR set; the next STATUS read clears it on the chip
  CHECK_EQ(sensor.ReadTemperatureC().error, Error::UnexpectedReset);
  CHECK(sensor.GetLatchedStatus().power_on_reset); // the internal read did not lose it
  REQUIRE(sensor.EnsureInitialized().ok());        // a new start: its own POR is not news
  CHECK(!sensor.GetLatchedStatus().power_on_reset);
  port.state.chip.PowerOn();
  const auto raw = sensor.ReadRegister(reg::kStatus); // the raw read latches too
  REQUIRE(raw.ok());
  CHECK((raw.value & reg::kStatusPor) != 0);
  CHECK(sensor.GetLatchedStatus().power_on_reset);
  sensor.ClearLatchedStatus();
  CHECK(!sensor.GetLatchedStatus().power_on_reset);
}

TEST_CASE("A bus error during a read is reported as Bus") {
  MinimalPort port;
  xmpl100::XMPL100<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  port.state.fail_transfer = port.state.transfers + 1; // the TEMP burst after STATUS
  CHECK_EQ(sensor.ReadTemperatureC().error, Error::Bus);
}

TEST_CASE("Error names come from ToString") {
  CHECK(xmpl100::ToString(Error::Ok)[0] == 'O');
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange): an unknown value on purpose
  CHECK(xmpl100::ToString(static_cast<Error>(200))[0] == 'U');
}
