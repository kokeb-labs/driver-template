/**
 * @file xmpl100.hpp
 * @brief XMPL100 driver: the one header to include.
 * @copyright Copyright (c) 2026 The driver-template authors. SPDX-License-Identifier: MIT
 *
 * @details
 * `xmpl100::XMPL100<Port>` drives one XMPL100 digital temperature sensor through a platform port
 * you write (see `xmpl100_spi_interface.hpp`). The driver has no platform code, allocates nothing,
 * throws nothing, takes no lock and is not thread-safe: give each object one owner, or guard it
 * with one mutex.
 */
#ifndef XMPL100_HPP
#define XMPL100_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "xmpl100_detail.hpp"
#include "xmpl100_registers.hpp"
#include "xmpl100_spi_interface.hpp"
#include "xmpl100_types.hpp"
#include "xmpl100_version.h"

namespace xmpl100 {

namespace detail {

// Optional hooks: a `Declares*` concept only tests the name, the `Has*` concept the exact
// signature, `noexcept` and the result type; the driver rejects a hook that is declared but does
// not match (KB-CPP20-007).

/// The port declares something called `GpioSet` (used to reject a misdeclared hook).
template <typename Port>
concept DeclaresGpioSet =
    requires(Port& port) { port.GpioSet(CtrlPin::Reset, GpioSignal::ACTIVE); };

/// The port drives control pins: `bool GpioSet(CtrlPin, GpioSignal) noexcept`.
template <typename Port>
concept HasGpioSet =
    requires { ExactHook<Port, bool(CtrlPin, GpioSignal) noexcept>::Take(&Port::GpioSet); };

/// The port declares something called `DelayUs`.
template <typename Port>
concept DeclaresDelayUs = requires(Port& port) { port.DelayUs(1_u32); };

/// The port waits: `void DelayUs(uint32_t us) noexcept`. A narrower parameter would truncate long
/// waits.
template <typename Port>
concept HasDelayUs = requires { ExactHook<Port, void(uint32_t) noexcept>::Take(&Port::DelayUs); };

/// The port declares something called `Log`.
template <typename Port>
concept DeclaresLog = requires(Port& port) { port.Log(LogLevel::Info, ""); };

/// The port takes log lines: `void Log(LogLevel, const char* message) noexcept`.
template <typename Port>
concept HasLog =
    requires { ExactHook<Port, void(LogLevel, const char*) noexcept>::Take(&Port::Log); };

/// Wait through the port. A port without `DelayUs()` fails to compile at the first call that needs
/// a wait: the chip's reset and conversion times are requirements, never skipped.
template <typename Port>
void DelayUs(Port& port, uint32_t us) noexcept {
  if constexpr (HasDelayUs<Port>) {
    port.DelayUs(us);
  } else {
    static_assert(kAlwaysFalse<Port>,
                  "xmpl100: the port must implement `void DelayUs(uint32_t us) noexcept`");
  }
}

/// Log through the port, or nothing when the port has no `Log()`. The driver never formats text.
template <typename Port>
void Log(Port& port, [[maybe_unused]] LogLevel level,
         [[maybe_unused]] const char* message) noexcept {
  if constexpr (HasLog<Port>) {
    port.Log(level, message);
  } else {
    (void)port;
  }
}

// Register encoders and the STATUS decoder: free constexpr functions, emitted once whatever the
// port type, and proved below with static_assert (KB-CPP20-009, -012).

/// CTRL for `config`, with START set when `start` is true (section 7.4).
[[nodiscard]] constexpr uint8_t CtrlValue(const Config& config, bool start) noexcept {
  auto value = reg::CtrlMode::Insert(0_u8, static_cast<uint32_t>(ToUnderlying(config.mode)));
  if (config.alert_enable) {
    value = static_cast<uint8_t>(static_cast<uint32_t>(value) | reg::kCtrlAlertEn);
  }
  if (config.alert_polarity == AlertPolarity::ActiveHigh) {
    value = static_cast<uint8_t>(static_cast<uint32_t>(value) | reg::kCtrlAlertPol);
  }
  if (start) {
    value = static_cast<uint8_t>(static_cast<uint32_t>(value) | reg::kCtrlStart);
  }
  return value;
}

/// CONFIG for `config` (section 7.5).
[[nodiscard]] constexpr uint8_t ConfigValue(const Config& config) noexcept {
  auto value = reg::ConfigOdr::Insert(0_u8, static_cast<uint32_t>(ToUnderlying(config.odr)));
  value = reg::ConfigAvg::Insert(value, static_cast<uint32_t>(ToUnderlying(config.averaging)));
  if (config.crc_enable) {
    value = static_cast<uint8_t>(static_cast<uint32_t>(value) | reg::kConfigCrcEn);
  }
  return value;
}

/// STATUS, decoded (section 7.3).
[[nodiscard]] constexpr Status DecodeStatus(uint8_t raw) noexcept {
  Status s{};
  const auto bits = static_cast<uint32_t>(raw);
  s.data_ready = (bits & reg::kStatusDrdy) != 0U;
  s.busy = (bits & reg::kStatusBusy) != 0U;
  s.alert_high = (bits & reg::kStatusAlertHi) != 0U;
  s.alert_low = (bits & reg::kStatusAlertLo) != 0U;
  s.power_on_reset = (bits & reg::kStatusPor) != 0U;
  s.fault = (bits & reg::kStatusFault) != 0U;
  return s;
}

// The reset configuration encodes to the reset values of CTRL and CONFIG (section 7).
static_assert(CtrlValue(Config{}, false) == reg::kCtrlReset);
static_assert(ConfigValue(Config{}) == reg::kConfigReset);
// Each field lands where the register map puts it, and decodes back.
static_assert(reg::ConfigAvg::Extract(ConfigValue(HighAccuracyPreset())) ==
              ToUnderlying(Averaging::X8));
static_assert(reg::ConfigOdr::Extract(ConfigValue(HighAccuracyPreset())) ==
              ToUnderlying(OutputDataRate::Hz8));
static_assert(reg::CtrlMode::Extract(CtrlValue(OneShotPreset(), true)) ==
              ToUnderlying(Mode::OneShot));
static_assert((static_cast<uint32_t>(CtrlValue(OneShotPreset(), true)) & reg::kCtrlStart) != 0U);
static_assert(DecodeStatus(reg::kStatusPor).power_on_reset && !DecodeStatus(reg::kStatusPor).fault);

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
 * Lifecycle: construct with the configuration (no I/O), then `EnsureInitialized()`, the one way
 * to start (bus up, reset, chip-ID check, configuration), use, optionally `Deinitialize()`
 * (shutdown mode). Every call that touches the chip returns `Error::NotInitialized` until
 * `EnsureInitialized()` has succeeded; no call starts the chip as a side effect.
 *
 * Optional port hooks, found at compile time:
 *
 * | Hook | Used for | Without it |
 * | --- | --- | --- |
 * | `void DelayUs(uint32_t us) noexcept` | t_RST after a reset, 1 ms between result polls | Compile error: the waits are required |
 * | `bool GpioSet(CtrlPin, GpioSignal) noexcept` | RESET_N: hardware reset in `EnsureInitialized()` and `HardwareReset()` | Soft reset in `EnsureInitialized()`; `HardwareReset()` returns `Unsupported` |
 * | `void Log(LogLevel, const char* message) noexcept` | One line per lifecycle event or failure | Nothing is logged |
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
  static_assert(
      !detail::DeclaresDelayUs<Port> || detail::HasDelayUs<Port>,
      "xmpl100: the DelayUs hook must be `void DelayUs(uint32_t us) noexcept`, so that no "
      "wait is truncated");
  static_assert(!detail::DeclaresLog<Port> || detail::HasLog<Port>,
                "xmpl100: the optional Log hook must be `void Log(LogLevel, const char* message) "
                "noexcept`");

public:
  // scaffold:bus-ctor
  /**
   * @brief Bind the driver to a port and store the configuration. No I/O, cannot fail.
   * @param port The port; it must outlive the driver.
   * @param config What `EnsureInitialized()` will apply; checked there, before any I/O.
   */
  explicit XMPL100(Port& port, const Config& config = {}) noexcept : bus_(port), config_(config) {}
  // /scaffold:bus-ctor

  /** @brief One driver object per chip. */
  XMPL100(const XMPL100&) = delete;
  /** @brief One driver object per chip. */
  XMPL100& operator=(const XMPL100&) = delete;

  /// @name Lifecycle
  /// @{

  /**
   * @brief Start the chip, once: the only way to initialise the driver.
   *
   * The first call brings the bus up, resets the chip (through RESET_N when the port has
   * `GpioSet()`, otherwise with CTRL.SOFT_RESET), checks CHIP_ID and applies the stored
   * configuration; it blocks for at most t_RST plus a few register accesses. After it succeeds,
   * later calls return `Ok` at once with no I/O. After it fails, the driver stays uninitialised and
   * the next call starts again from the beginning.
   *
   * @return `Ok`, `InvalidArgument` (the stored configuration fails `Validate()`; no I/O), `Bus`,
   *         `Timeout` (reset did not finish), `DeviceIdMismatch`, or `Unsupported` (CRC asked for
   *         on revision A).
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after a successful `EnsureInitialized()` and before `Deinitialize()`. No I/O. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  /**
   * @brief Put the chip in shutdown, its low-power safe state, and mark the driver uninitialised.
   *
   * The driver is uninitialised afterwards even if the write failed; the next
   * `EnsureInitialized()` starts the chip again (the recovery after a fault). On an uninitialised
   * driver it does nothing and returns `Ok`.
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
   * @brief Change the whole configuration.
   *
   * Running: the configuration is written to the chip. Not started yet: `config` is only stored,
   * for the first `EnsureInitialized()`, with no I/O.
   *
   * @param config Checked with `Validate()` before any I/O.
   * @return `Ok`, `InvalidArgument`, `Unsupported` (CRC on revision A) or `Bus`.
   */
  Result<void> ApplyConfig(const Config& config) noexcept;

  /** @return The configuration last applied or stored. No I/O. */
  [[nodiscard]] const Config& GetConfig() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary driver: the reference would dangle (KB-CPP20-044). */
  void GetConfig() const&& = delete;

  /**
   * @brief Change the conversion mode only; stored without I/O before the start, as
   * `ApplyConfig()`.
   * @param mode The mode.
   * @return `Ok`, `InvalidArgument` (not an enumerator, or averaging too long
   *         for the data rate in continuous mode) or `Bus`.
   */
  Result<void> SetMode(Mode mode) noexcept;

  /**
   * @brief Set the alert thresholds; stored without I/O before the start, as `ApplyConfig()`.
   * @param low_c Low threshold in °C, from `kAlertMinC`.
   * @param high_c High threshold in °C, up to `kAlertMaxC`; must be above `low_c`.
   * @return `Ok`, `InvalidArgument` or `Bus`.
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
  /// @name Diagnostics
  /// @{

  /**
   * @brief The clear-on-read STATUS flags (POR) seen by any STATUS read since the last
   * `ClearLatchedStatus()`, the driver's own reads included. No I/O.
   *
   * Every STATUS read the driver makes (`ReadStatus()`, the polls inside `WaitDataReady()`,
   * `TriggerOneShot()` and the reset sequence, a raw `ReadRegister(reg::kStatus)`) ORs the bits it
   * clears on the chip into this latch, so no internal read hides a flag from the caller (handbook
   * KB-CPP-119). The start-up empties it: the POR of the driver's own reset is not news.
   *
   * @return The latched flags; only `power_on_reset` can be set.
   */
  [[nodiscard]] Status GetLatchedStatus() const noexcept {
    return detail::DecodeStatus(latched_status_);
  }

  /** @brief Empty the latch of `GetLatchedStatus()`. No I/O. */
  void ClearLatchedStatus() noexcept {
    latched_status_ = 0;
  }

  /**
   * @return The raw STATUS byte of the last STATUS read, by any call; 0 before the first. No I/O
   * (handbook KB-CPP-129).
   */
  [[nodiscard]] uint8_t GetLastStatusByte() const noexcept {
    return last_status_;
  }

  /**
   * @brief Read one register once: one transfer, no retry, no decoding (handbook KB-CPP-129).
   *
   * For bring-up and diagnostics. Side effects are the chip's own: reading STATUS clears POR on the
   * chip (the driver latches it, see `GetLatchedStatus()`), reading TEMP_LSB clears DRDY.
   *
   * @param address A register address, 0 to `reg::kLastRegister`.
   * @return The register value, or `NotInitialized`, `InvalidArgument` (no such register; nothing
   *         sent) or `Bus`.
   */
  Result<uint8_t> ReadRegister(uint8_t address) noexcept;

  /// @}
  /// @name Identification
  /// @{

  /** @return REV_ID read by the last successful start or reset; 0 before. No I/O. */
  [[nodiscard]] uint8_t GetRevision() const noexcept {
    return revision_;
  }

  /// @}

private:
  Result<void> initialize() noexcept;
  Result<void> resetAndConfigure(const Config& config, bool soft_reset) noexcept;
  Result<void> pulseResetPin() noexcept;
  Result<void> waitResetDone() noexcept;
  Result<void> writeConfigRegisters(const Config& config) noexcept;
  Result<uint8_t> readRegister(uint8_t reg) noexcept;
  Result<uint8_t> readStatusByte() noexcept;
  Result<void> writeRegister(uint8_t reg, uint8_t value) noexcept;
  Result<void> fail(Error error, const char* message) noexcept;

  /// STATUS reads after the t_RST wait before a reset counts as stuck.
  static constexpr uint32_t kResetPolls = 3U;

  detail::RegisterBus<Port> bus_;
  Config config_{};
  uint8_t revision_{0};
  uint8_t last_status_{0};    ///< The last STATUS byte read (GetLastStatusByte()).
  uint8_t latched_status_{0}; ///< Clear-on-read STATUS bits seen since ClearLatchedStatus().
  bool initialized_{false};
};

/** @} */

} // namespace xmpl100

// codeql::misra_deviation_next_line(MD-CPP-009)
#include "../src/xmpl100.ipp" // IWYU pragma: export

#endif // XMPL100_HPP
