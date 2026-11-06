/**
 * @file mock_xmpl100_bus.hpp
 * @brief SPI mock ports over the register model. The frame decoding here is the only bus-specific
 * part of the host tests.
 *
 * `MockPortBase<Self>` implements the required hooks of the SPI port over a `MockBusState`. Derive
 * from it to build a port with any set of optional hooks:
 *
 * | Port | Hooks |
 * | --- | --- |
 * | `MinimalPort` | the required ones plus `DelayUs()` (needed by every wait) |
 * | `FullPort` | also `GpioSet()` (RESET_N) and `Log()` |
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "mock_xmpl100.hpp"
#include "xmpl100_spi_interface.hpp"
#include "xmpl100_types.hpp"

namespace xmpl100_test {

/// Shared state of the mock ports: the chip, fault injection and counters.
struct MockBusState {
  Xmpl100Model chip;
  bool bus_up_ok = true;  ///< EnsureInitialized() result.
  int fail_transfer = -1; ///< Fail the transfer with this index (0-based); -1 never.
  int transfers = 0;      ///< Transfers so far.
  int bus_up_calls = 0;   ///< EnsureInitialized() calls so far.

  /// Apply one SPI frame to the model: first byte bit 7 = read, bits 6..0 = address.
  bool DoTransfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    const int index = transfers++;
    if (index == fail_transfer || len == 0 || tx == nullptr) {
      return false;
    }
    const bool read = (tx[0] & 0x80U) != 0;
    const auto address = static_cast<uint8_t>(tx[0] & 0x7FU);
    if (rx != nullptr) {
      rx[0] = 0;
    }
    for (std::size_t i = 1; i < len; ++i) {
      const auto a = static_cast<uint8_t>(address + i - 1);
      if (read) {
        const uint8_t v =
            chip.Read(a); // NOLINT(cppcoreguidelines-init-variables): initialised here
        if (rx != nullptr) {
          rx[i] = v;
        }
      } else {
        chip.Write(a, tx[i]);
      }
    }
    return true;
  }
};

/// The required SPI hooks over a MockBusState. Derive `class P : public MockPortBase<P>`.
template <typename Self>
class MockPortBase : public xmpl100::SpiInterface<Self> {
public:
  MockBusState state;

  bool EnsureInitialized() noexcept {
    state.bus_up_calls++;
    return state.bus_up_ok;
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return state.DoTransfer(tx, rx, len);
  }
};

/// Required hooks plus the delay; time passes in the model as the driver waits.
class MinimalPort : public MockPortBase<MinimalPort> {
public:
  uint64_t delayed_us = 0; ///< Sum of DelayUs() arguments.

  void DelayUs(uint32_t us) noexcept {
    delayed_us += us;
    state.chip.Elapse(us);
  }
};

/// Every optional hook, with counters.
class FullPort : public MockPortBase<FullPort> {
public:
  uint64_t delayed_us = 0; ///< Sum of DelayUs() arguments.
  int log_lines = 0;       ///< Log() calls.
  int error_lines = 0;     ///< Log() calls at LogLevel::Error.
  bool gpio_ok = true;     ///< GpioSet() result.
  bool reset_asserted = false;
  int reset_pulses = 0;

  void DelayUs(uint32_t us) noexcept {
    delayed_us += us;
    state.chip.Elapse(us);
  }
  bool GpioSet(xmpl100::CtrlPin pin, xmpl100::GpioSignal signal) noexcept {
    if (!gpio_ok || pin != xmpl100::CtrlPin::Reset) {
      return false;
    }
    const bool assert_reset = signal == xmpl100::GpioSignal::ACTIVE;
    if (reset_asserted && !assert_reset) {
      state.chip.Reset(); // released: the chip runs its reset sequence
      reset_pulses++;
    }
    reset_asserted = assert_reset;
    return true;
  }
  void Log(xmpl100::LogLevel level, const char* fmt, ...) noexcept {
    (void)fmt;
    log_lines++;
    if (level == xmpl100::LogLevel::Error) {
      error_lines++;
    }
  }
};

} // namespace xmpl100_test
