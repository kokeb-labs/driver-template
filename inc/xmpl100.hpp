/**
 * @file xmpl100.hpp
 * @brief XMPL100 driver: the one header to include.
 * @copyright Copyright (c) 2026 The Kokeb Labs Authors. SPDX-License-Identifier: MIT
 *
 * @details
 * `xmpl100::XMPL100<Port>` drives one XMPL100 digital temperature sensor through a platform port
 * you write (see `xmpl100_spi_interface.hpp`). The driver has no platform code, allocates nothing,
 * throws nothing, takes no lock and is not thread-safe: give each object one owner, or guard it
 * with one mutex.
 */
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "xmpl100_registers.hpp"
#include "xmpl100_spi_interface.hpp"
#include "xmpl100_types.hpp"
#include "xmpl100_version.h"

namespace xmpl100 {

namespace detail {

/// The port declares something called `GpioSet` (used to reject a misdeclared hook).
template <typename Port>
concept DeclaresGpioSet =
    requires(Port& port) { port.GpioSet(CtrlPin::Reset, GpioSignal::ACTIVE); };

/// The port drives control pins: `bool GpioSet(CtrlPin, GpioSignal) noexcept`.
template <typename Port>
concept HasGpioSet = requires(Port& port) {
  { port.GpioSet(CtrlPin::Reset, GpioSignal::ACTIVE) } -> std::same_as<bool>;
};

/// The port takes log lines: `void Log(LogLevel, const char* fmt, ...) noexcept`.
template <typename Port>
concept HasLog = requires(Port& port, const char* fmt) { port.Log(LogLevel::Info, fmt); };

/// Wait through the port. A port without `DelayUs()` fails to compile at the first call that needs
/// a wait: the chip's reset and conversion times are requirements, never skipped.
template <typename Port>
void DelayUs(Port& port, uint32_t us) noexcept {
  if constexpr (requires { port.DelayUs(us); }) {
    port.DelayUs(us);
  } else {
    static_assert(sizeof(Port) == 0,
                  "xmpl100: the port must implement `void DelayUs(uint32_t us) noexcept`");
  }
}

/// Log through the port, or nothing when the port has no `Log()`. The driver never formats text.
template <typename Port>
void Log(Port& port, LogLevel level, const char* message) noexcept {
  if constexpr (HasLog<Port>) {
    port.Log(level, message);
  } else {
    (void)port;
    (void)level;
    (void)message;
  }
}

} // namespace detail

/**
 * @defgroup xmpl100_driver XMPL100 driver
 * @brief The driver class.
 * @{
 */

// clang-format off: the table below must keep one row per line.
/**
 * @brief Driver for one XMPL100 on a platform port.
 *
 * Lifecycle: construct (no I/O), `Initialize()` (bus up, reset, chip-ID check, configuration),
 * use, optionally `Deinitialize()` (shutdown mode). Every call that touches the chip returns
 * `Error::NotInitialized` until `Initialize()` has succeeded.
 *
 * Optional port hooks, found at compile time:
 *
 * | Hook | Used for | Without it |
 * | --- | --- | --- |
 * | `void DelayUs(uint32_t us) noexcept` | t_RST after a reset, 1 ms between result polls | Compile error: the waits are required |
 * | `bool GpioSet(CtrlPin, GpioSignal) noexcept` | RESET_N: hardware reset in `Initialize()` and `HardwareReset()` | Soft reset in `Initialize()`; `HardwareReset()` returns `Unsupported` |
 * | `void Log(LogLevel, const char* fmt, ...) noexcept` | One line per lifecycle event or failure | Nothing is logged |
 *
 * A hook declared with the wrong signature (for example `void GpioSet(...)`) is a compile error.
 *
 * @tparam Port Your class derived from `SpiInterface<Port>`.
 */
// clang-format on
template <SpiPort Port>
class XMPL100 {
  static_assert(!detail::DeclaresGpioSet<Port> || detail::HasGpioSet<Port>,
                "xmpl100: the optional GpioSet hook must be `bool GpioSet(CtrlPin, GpioSignal) "
                "noexcept`, so that a failed pin write is reported");

public:
  // scaffold:bus-ctor
  /**
   * @brief Bind the driver to a port. No I/O.
   * @param port The port; it must outlive the driver.
   */
  explicit XMPL100(Port& port) noexcept : bus_(port) {}
  // /scaffold:bus-ctor

  /** @brief One driver object per chip. */
  XMPL100(const XMPL100&) = delete;
  /** @brief One driver object per chip. */
  XMPL100& operator=(const XMPL100&) = delete;

  /// @name Lifecycle
  /// @{

  /**
   * @brief Bring the bus up, reset the chip, check CHIP_ID and apply `config`.
   *
   * Resets through RESET_N when the port has `GpioSet()`, otherwise with CTRL.SOFT_RESET. Safe to
   * call again: it resets and reconfigures the chip each time. Blocks for at most t_RST plus a few
   * register accesses.
   *
   * @param config What to write; checked with `Validate()` before any I/O.
   * @return `Ok`, `InvalidArgument`, `Bus`, `Timeout` (reset did not finish), `DeviceIdMismatch`,
   *         or `Unsupported` (CRC asked for on revision A).
   */
  Result<void> Initialize(const Config& config = {}) noexcept;

  /**
   * @brief `Initialize()` with the last configuration, unless already initialised.
   * @return `Ok` at once when initialised; otherwise what `Initialize()` returns.
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after a successful `Initialize()` and before `Deinitialize()`. No I/O. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  /**
   * @brief Put the chip in shutdown, its low-power safe state, and mark the driver uninitialised.
   *
   * The driver is uninitialised afterwards even if the write failed. On an uninitialised driver it
   * does nothing and returns `Ok`.
   *
   * @return `Ok` or `Bus`.
   */
  Result<void> Deinitialize() noexcept;

  /**
   * @brief Soft reset (CTRL.SOFT_RESET), then re-apply the current configuration.
   * @return `Ok`, `NotInitialized`, `Bus`, `Timeout`, `DeviceIdMismatch` or `Unsupported`.
   */
  Result<void> SoftReset() noexcept;

  /**
   * @brief Pulse RESET_N for `kResetPulseMinUs`, then re-apply the current configuration.
   * @return `Ok`, `NotInitialized`, `Unsupported` (the port has no `GpioSet()`), `Bus` (including
   *         a failed pin write), `Timeout` or `DeviceIdMismatch`.
   */
  Result<void> HardwareReset() noexcept;

  /// @}
  /// @name Configuration
  /// @{

  /**
   * @brief Write a whole configuration.
   * @param config Checked with `Validate()` before any I/O.
   * @return `Ok`, `NotInitialized`, `InvalidArgument`, `Unsupported` (CRC on revision A) or `Bus`.
   */
  Result<void> ApplyConfig(const Config& config) noexcept;

  /** @return The configuration last applied (or passed to `Initialize()`). No I/O. */
  [[nodiscard]] const Config& GetConfig() const noexcept {
    return config_;
  }

  /**
   * @brief Change the conversion mode only.
   * @param mode The mode.
   * @return `Ok`, `NotInitialized`, `InvalidArgument` (not an enumerator, or averaging too long
   *         for the data rate in continuous mode) or `Bus`.
   */
  Result<void> SetMode(Mode mode) noexcept;

  /**
   * @brief Set the alert thresholds.
   * @param low_c Low threshold in °C, from `kAlertMinC`.
   * @param high_c High threshold in °C, up to `kAlertMaxC`; must be above `low_c`.
   * @return `Ok`, `NotInitialized`, `InvalidArgument` or `Bus`.
   */
  Result<void> SetAlertThresholdsC(float low_c, float high_c) noexcept;

  /// @}
  /// @name Measurement
  /// @{

  /**
   * @brief Read and decode STATUS. Clears STATUS.POR on the chip.
   * @return The status, or `NotInitialized` / `Bus`.
   */
  Result<Status> ReadStatus() noexcept;

  /**
   * @brief Read the latest result in counts (1/128 °C), offset included.
   *
   * Checks STATUS first: FAULT gives `DeviceFault`; POR gives `UnexpectedReset` and leaves the
   * driver uninitialised, so that `EnsureInitialized()` reconfigures the chip. With CRC enabled the
   * CRC-8 of the result is checked.
   *
   * @return The counts, or `NotInitialized`, `Bus`, `DeviceFault`, `UnexpectedReset` or `Crc`.
   */
  Result<int16_t> ReadRawTemperature() noexcept;

  /**
   * @brief Read the latest result in degrees Celsius.
   * @return The temperature, or the errors of `ReadRawTemperature()`.
   */
  Result<float> ReadTemperatureC() noexcept;

  /**
   * @brief Start one conversion (mode `Shutdown` or `OneShot`).
   * @return `Ok`, `NotInitialized`, `Busy` (continuous mode, or a conversion still running) or
   *         `Bus`.
   */
  Result<void> TriggerOneShot() noexcept;

  /**
   * @brief Wait for STATUS.DRDY, polling once per millisecond for at most
   * `Config::conversion_timeout_ms`.
   * @return `Ok`, `NotInitialized`, `Timeout`, `DeviceFault` or `Bus`.
   */
  Result<void> WaitDataReady() noexcept;

  /**
   * @brief Trigger one conversion, wait for it and read it.
   * @return The temperature in °C, or the errors of `TriggerOneShot()`, `WaitDataReady()` and
   *         `ReadTemperatureC()`.
   */
  Result<float> MeasureOnceC() noexcept;

  /// @}
  /// @name Identification
  /// @{

  /** @return REV_ID read by the last successful `Initialize()`; 0 before. No I/O. */
  [[nodiscard]] uint8_t GetRevision() const noexcept {
    return revision_;
  }

  /// @}

private:
  Result<void> resetAndConfigure(const Config& config, bool soft_reset) noexcept;
  Result<void> pulseResetPin() noexcept;
  Result<void> waitResetDone() noexcept;
  Result<void> writeConfigRegisters(const Config& config) noexcept;
  Result<uint8_t> readRegister(uint8_t reg) noexcept;
  Result<void> writeRegister(uint8_t reg, uint8_t value) noexcept;
  Result<void> fail(Error error, const char* message) noexcept;

  static uint8_t ctrlValue(const Config& config, bool start) noexcept;
  static uint8_t configValue(const Config& config) noexcept;
  static int16_t toCounts(float celsius, int32_t counts_per_degree) noexcept;

  /// STATUS reads after the t_RST wait before a reset counts as stuck.
  static constexpr uint32_t kResetPolls = 3;

  detail::RegisterBus<Port> bus_;
  Config config_{};
  uint8_t revision_{0};
  bool initialized_{false};
};

/** @} */

} // namespace xmpl100

#include "../src/xmpl100.ipp" // IWYU pragma: export
