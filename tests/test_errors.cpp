/**
 * @file test_errors.cpp
 * @brief The `Error` enum follows the standard, and its values are returned by real calls.
 *
 * - The base set (0 to 9) has the names and values every driver of the org shares.
 * - Every chip-specific value (16 and up) MUST be returned by some call: the enumerators are found
 *   through `ToString()` (-Wswitch keeps it complete), so a new value fails this test until a
 *   scenario below produces it.
 * - Base values MAY go unreturned; `kRequireEveryBaseValue` says whether this driver returns them
 *   all (the XMPL100 does). Set it to false, and list the unreturned ones on the API guide, for a
 *   chip without, say, a CRC.
 */
#include <cstring>

#include "mock_xmpl100.hpp"
#include "test_harness.hpp"
#include "xmpl100.hpp"

using xmpl100::Error;
using xmpl100_test::FullPort;
using xmpl100_test::MinimalPort;

static_assert(static_cast<int>(Error::Ok) == 0 && static_cast<int>(Error::NotInitialized) == 1 &&
                  static_cast<int>(Error::InvalidArgument) == 2 &&
                  static_cast<int>(Error::Bus) == 3 && static_cast<int>(Error::Timeout) == 4 &&
                  static_cast<int>(Error::DeviceIdMismatch) == 5 &&
                  static_cast<int>(Error::Crc) == 6 && static_cast<int>(Error::Busy) == 7 &&
                  static_cast<int>(Error::DeviceFault) == 8 &&
                  static_cast<int>(Error::Unsupported) == 9,
              "the base Error set has fixed values (standards/driver.md)");

namespace {

constexpr bool kRequireEveryBaseValue = true;
constexpr int kFirstChipSpecific = 16;

bool g_seen[256] = {};

void Record(Error e) {
  g_seen[static_cast<uint8_t>(e)] = true;
}

void RunScenarios() {
  { // Ok and NotInitialized
    MinimalPort port;
    xmpl100::XMPL100<MinimalPort> s(port);
    Record(s.ReadTemperatureC().error);
    Record(s.Initialize().error);
  }
  { // InvalidArgument
    MinimalPort port;
    xmpl100::XMPL100<MinimalPort> s(port);
    xmpl100::Config c{};
    c.conversion_timeout_ms = 0;
    Record(s.Initialize(c).error);
  }
  { // Bus
    MinimalPort port;
    port.state.bus_up_ok = false;
    xmpl100::XMPL100<MinimalPort> s(port);
    Record(s.Initialize().error);
  }
  { // Timeout
    MinimalPort port;
    port.state.chip.reset_stuck = true;
    xmpl100::XMPL100<MinimalPort> s(port);
    Record(s.Initialize().error);
  }
  { // DeviceIdMismatch
    MinimalPort port;
    port.state.chip.chip_id = 0x00;
    xmpl100::XMPL100<MinimalPort> s(port);
    Record(s.Initialize().error);
  }
  { // Crc
    MinimalPort port;
    port.state.chip.corrupt_crc = true;
    xmpl100::XMPL100<MinimalPort> s(port);
    if (s.Initialize(xmpl100::HighAccuracyPreset()).ok()) {
      Record(s.ReadTemperatureC().error);
    }
  }
  { // Busy
    MinimalPort port;
    xmpl100::XMPL100<MinimalPort> s(port);
    if (s.Initialize().ok()) {
      Record(s.TriggerOneShot().error);
    }
  }
  { // DeviceFault
    MinimalPort port;
    port.state.chip.fault = true;
    xmpl100::XMPL100<MinimalPort> s(port);
    if (s.Initialize().ok()) {
      Record(s.ReadTemperatureC().error);
    }
  }
  { // Unsupported
    MinimalPort port;
    port.state.chip.rev_id = xmpl100::kRevisionA;
    xmpl100::XMPL100<MinimalPort> s(port);
    Record(s.Initialize(xmpl100::HighAccuracyPreset()).error);
  }
  { // Unsupported, through the missing GpioSet hook
    MinimalPort port;
    xmpl100::XMPL100<MinimalPort> s(port);
    if (s.Initialize().ok()) {
      Record(s.HardwareReset().error);
    }
  }
  { // UnexpectedReset (chip-specific)
    MinimalPort port;
    xmpl100::XMPL100<MinimalPort> s(port);
    if (s.Initialize().ok()) {
      port.state.chip.PowerOn();
      Record(s.ReadTemperatureC().error);
    }
  }
}

} // namespace

TEST_CASE("Every chip-specific Error value is returned by some call") {
  RunScenarios();
  int enumerators = 0;
  for (int v = 0; v < 256; ++v) {
    const auto e = static_cast<Error>(v);
    if (std::strcmp(xmpl100::ToString(e), "Unknown") == 0) {
      continue;
    }
    ++enumerators;
    const bool required = v >= kFirstChipSpecific || kRequireEveryBaseValue;
    if (!g_seen[v]) {
      std::printf("    %s: Error::%s is never returned\n", required ? "FAIL" : "note",
                  xmpl100::ToString(e));
    }
    CHECK(g_seen[v] || !required);
    CHECK(v < 10 || v >= kFirstChipSpecific); // 10 to 15 are reserved for the base set
  }
  CHECK(enumerators >= 2);
}
