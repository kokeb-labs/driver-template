/**
 * @file xmpl100_types.hpp
 * @brief Public types of the XMPL100 driver: errors, `Result<T>`, configuration and presets.
 * @copyright Copyright (c) 2026 The driver-template authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is plain data with no platform dependency, so handlers, tests and tools can
 * include it without the driver template. The configuration defaults are the chip's reset values
 * (XMPL100 datasheet, section 7, register map), so `Config{}` describes a chip that was just reset.
 */
#ifndef XMPL100_TYPES_HPP
#define XMPL100_TYPES_HPP

#include <cstdint>

#include "xmpl100_detail.hpp"
#include "xmpl100_version.h"

/** @brief The XMPL100 driver: everything it declares lives here; internals in `detail`. */
namespace xmpl100 {

/**
 * @defgroup xmpl100_types Driver types
 * @brief Errors, results, configuration and presets.
 * @{
 */

/**
 * @brief Why a call failed. Every call of the driver returns one of these in its `Result`.
 *
 * Values 0 to 9 are the shared base set; chip-specific values start at 16. `[[nodiscard]]` on
 * the type: a function that returns a bare `Error` cannot have it ignored either (KB-CPP20-037).
 */
enum class [[nodiscard]] Error : uint8_t {
  // Base set: the same names and values in every driver of the org (standards/driver.md).
  Ok = 0,               ///< The call succeeded.
  NotInitialized = 1,   ///< `EnsureInitialized()` has not succeeded, or `Deinitialize()` ran since.
  InvalidArgument = 2,  ///< An argument or a `Config` field is out of range; nothing was sent.
  Bus = 3,              ///< A port hook returned `false`.
  Timeout = 4,          ///< The chip did not finish a reset or a conversion in the allowed time.
  DeviceIdMismatch = 5, ///< CHIP_ID did not read `kChipId`: wrong chip, no chip or a wiring fault.
  Crc = 6,              ///< The CRC-8 of a temperature read did not match (CONFIG.CRC_EN = 1).
  Busy = 7,             ///< The chip is converting: a one-shot is pending or continuous mode runs.
  DeviceFault = 8,      ///< STATUS.FAULT is set: the sensing element is open or shorted.
  Unsupported = 9,      ///< The hook or silicon feature this call needs is absent.
  // Chip-specific values start at 16; each one is returned by some call (tests/test_errors.cpp).
  UnexpectedReset = 16, ///< STATUS.POR is set: the chip reset since the start (brown-out).
};

/**
 * @brief The name of an error, for logs.
 * @param error The error.
 * @return A static string such as `"Timeout"`; `"Unknown"` for a value that is not an enumerator.
 */
constexpr const char* ToString(Error error) noexcept {
  switch (error) {
  case Error::Ok:
    return "Ok";
  case Error::NotInitialized:
    return "NotInitialized";
  case Error::InvalidArgument:
    return "InvalidArgument";
  case Error::Bus:
    return "Bus";
  case Error::Timeout:
    return "Timeout";
  case Error::DeviceIdMismatch:
    return "DeviceIdMismatch";
  case Error::Crc:
    return "Crc";
  case Error::Busy:
    return "Busy";
  case Error::DeviceFault:
    return "DeviceFault";
  case Error::Unsupported:
    return "Unsupported";
  case Error::UnexpectedReset:
    return "UnexpectedReset";
  default:
    break;
  }
  return "Unknown";
}

/**
 * @brief The value of a call, or why there is none.
 *
 * Check `ok()` (or the `bool` conversion) before reading `value`; on failure `value` is `T{}`.
 *
 * @tparam T The value type. `Result<void>` carries only the error.
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};              ///< The value; `T{}` when the call failed.
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /**
   * @brief A successful result.
   * @param v The value.
   * @return `{v, Error::Ok}`.
   */
  static constexpr Result Success(T v) noexcept {
    return {v, Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{T{}, e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {T{}, e};
  }
};

/** @brief The result of a call that returns no value. */
template <>
struct [[nodiscard]] Result<void> {
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }

  /** @return A successful result. */
  static constexpr Result Success() noexcept {
    return {Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {e};
  }
};

/** @brief Severity passed to the optional `Log()` hook. */
enum class LogLevel : uint8_t {
  Error = 0, ///< A call failed.
  Warning,   ///< Something unexpected that the driver recovered from.
  Info,      ///< Lifecycle events: initialised, reset.
  Debug,     ///< Detail for bring-up.
};

/** @brief Conversion mode (CTRL.MODE). */
enum class Mode : uint8_t {
  Shutdown = 0,   ///< No conversions; lowest supply current.
  OneShot = 1,    ///< One conversion per `TriggerOneShot()`, then back to shutdown.
  Continuous = 2, ///< Conversions at the output data rate. Reset value.
};

/** @brief Output data rate in continuous mode (CONFIG.ODR). */
enum class OutputDataRate : uint8_t {
  Hz1 = 0,  ///< 1 conversion per second.
  Hz2 = 1,  ///< 2 per second.
  Hz4 = 2,  ///< 4 per second. Reset value.
  Hz8 = 3,  ///< 8 per second.
  Hz16 = 4, ///< 16 per second.
};

/** @brief Samples averaged into one result (CONFIG.AVG). */
enum class Averaging : uint8_t {
  X1 = 0, ///< No averaging. Reset value.
  X2 = 1, ///< 2 samples.
  X4 = 2, ///< 4 samples.
  X8 = 3, ///< 8 samples.
};

/** @brief Polarity of the ALERT output (CTRL.ALERT_POL). */
enum class AlertPolarity : uint8_t {
  ActiveLow = 0,  ///< ALERT pulls low when a threshold is crossed. Reset value.
  ActiveHigh = 1, ///< ALERT drives high.
};

/** @brief STATUS register, decoded. Reading it clears `power_on_reset`. */
struct Status {
  bool data_ready{false};     ///< DRDY: a new result is waiting; cleared by reading it.
  bool busy{false};           ///< BUSY: a reset or a one-shot conversion is in progress.
  bool alert_high{false};     ///< ALERT_HI: the last result was above the high threshold.
  bool alert_low{false};      ///< ALERT_LO: the last result was below the low threshold.
  bool power_on_reset{false}; ///< POR: the chip reset since STATUS was last read.
  bool fault{false};          ///< FAULT: the sensing element is open or shorted.
};

/// CHIP_ID register value of every XMPL100.
inline constexpr uint8_t kChipId = 0xA1;
/// REV_ID of revision A silicon.
inline constexpr uint8_t kRevisionA = 0x10;
/// REV_ID of revision B silicon, the first with CRC-protected reads.
inline constexpr uint8_t kRevisionB = 0x20;
/// Temperature register resolution: 128 counts per degree Celsius (1 LSB = 0.0078125 °C).
inline constexpr int32_t kCountsPerDegreeC = 128;
/// OFFSET register resolution: 16 counts per degree Celsius (1 LSB = 0.0625 °C).
inline constexpr int32_t kOffsetCountsPerDegreeC = 16;
/// Lowest alert threshold the driver accepts, in degrees Celsius.
inline constexpr float kAlertMinC = -55.0F;
/// Highest alert threshold the driver accepts, in degrees Celsius.
inline constexpr float kAlertMaxC = 150.0F;
/// Lowest offset the OFFSET register holds, in degrees Celsius (-128 counts).
inline constexpr float kOffsetMinC = -8.0F;
/// Highest offset the OFFSET register holds, in degrees Celsius (+127 counts).
inline constexpr float kOffsetMaxC = 7.9375F;
/// Longest single conversion, in microseconds (datasheet t_CONV, maximum).
inline constexpr uint32_t kConversionTimeMaxUs = 10000;
/// Longest soft or hardware reset, in microseconds (datasheet t_RST, maximum).
inline constexpr uint32_t kResetTimeMaxUs = 1000;
/// Shortest RESET_N low pulse, in microseconds (datasheet t_RST_PW, minimum).
inline constexpr uint32_t kResetPulseMinUs = 10;
/// Longest `Config::conversion_timeout_ms` the driver accepts: it bounds `WaitDataReady()`
/// (KB-CPP20-058; the Kconfig range is the same).
inline constexpr uint32_t kConversionTimeoutMaxMs = 10000;

/**
 * @brief The resolution of a temperature register: counts per degree Celsius. A strong type, so a
 * conversion cannot be handed a plain integer in the wrong place (KB-CPP20-030).
 */
struct Resolution {
  int32_t counts_per_degree_c{1}; ///< Counts per °C; positive.
};
/// TEMP, ALERT_HIGH and ALERT_LOW: 1/128 °C per count.
inline constexpr Resolution kTemperatureResolution{kCountsPerDegreeC};
/// OFFSET: 1/16 °C per count.
inline constexpr Resolution kOffsetResolution{kOffsetCountsPerDegreeC};

/**
 * @brief Everything `EnsureInitialized()` and `ApplyConfig()` write to the chip.
 *
 * The defaults are the reset values of the datasheet, so `Config{}` changes nothing on a freshly
 * reset chip except the driver-side timeouts. Fields with a unit carry it as a suffix. It has no
 * `operator==`: its `float` members would compare exactly (KB-CPP20-033).
 */
struct Config {
  Mode mode{Mode::Continuous};             ///< CTRL.MODE. Reset value: continuous.
  OutputDataRate odr{OutputDataRate::Hz4}; ///< CONFIG.ODR. Reset value: 4 Hz.
  Averaging averaging{Averaging::X1};      ///< CONFIG.AVG. Reset value: no averaging.
  bool crc_enable{false};                  ///< CONFIG.CRC_EN; needs revision B. Reset value: off.
  bool alert_enable{false};                ///< CTRL.ALERT_EN. Reset value: off.
  AlertPolarity alert_polarity{
      AlertPolarity::ActiveLow};       ///< CTRL.ALERT_POL. Reset value: active low.
  float alert_low_c{-40.0F};           ///< ALERT_LOW threshold, °C. Reset value: -40 °C.
  float alert_high_c{125.0F};          ///< ALERT_HIGH threshold, °C. Reset value: +125 °C.
  float offset_c{0.0F};                ///< OFFSET added to every result, °C, -8 to +7.9375.
  uint32_t conversion_timeout_ms{100}; ///< Driver side: longest wait for a result, 1 to
                                       ///< `kConversionTimeoutMaxMs`.
};

/**
 * @brief Lowest supply current: one conversion per second, no averaging.
 * @return The preset.
 */
constexpr Config LowPowerPreset() noexcept {
  Config c{};
  c.odr = OutputDataRate::Hz1;
  c.averaging = Averaging::X1;
  return c;
}

/**
 * @brief Least noise: 8 samples averaged at 8 Hz, CRC-checked reads (revision B).
 * @return The preset.
 */
constexpr Config HighAccuracyPreset() noexcept {
  Config c{};
  c.odr = OutputDataRate::Hz8;
  c.averaging = Averaging::X8;
  c.crc_enable = true;
  return c;
}

/**
 * @brief Conversions only on request: shutdown between `MeasureOnceC()` calls.
 * @return The preset.
 */
constexpr Config OneShotPreset() noexcept {
  Config c{};
  c.mode = Mode::OneShot;
  c.averaging = Averaging::X2;
  return c;
}

/**
 * @brief Number of samples a setting averages.
 * @param averaging The setting.
 * @return 1, 2, 4 or 8.
 */
constexpr uint32_t SamplesPerResult(Averaging averaging) noexcept {
  // The mask keeps the shift in range for any value the enumeration can hold (KB-CPP20-020).
  return uint32_t{1} << (static_cast<uint32_t>(averaging) & 0x03U);
}

/**
 * @brief Result period of an output data rate, in microseconds.
 * @param odr The rate.
 * @return 1000000 / rate.
 */
constexpr uint32_t PeriodUs(OutputDataRate odr) noexcept {
  // The mask keeps the shift in range for any value the enumeration can hold (KB-CPP20-020).
  return uint32_t{1000000} >> (static_cast<uint32_t>(odr) & 0x07U);
}

/**
 * @brief A temperature in register counts, rounded half away from zero.
 * @param celsius The temperature in °C.
 * @param resolution The register's resolution (`kTemperatureResolution`, `kOffsetResolution`).
 * @return The counts, or `InvalidArgument` when the value is NaN or outside the 16-bit register.
 */
constexpr Result<int16_t> ToCounts(float celsius, Resolution resolution) noexcept {
  const float scaled = celsius * static_cast<float>(resolution.counts_per_degree_c);
  // "Not in range" on purpose: a NaN fails every comparison, so it is rejected here.
  // NOLINTNEXTLINE(readability-simplify-boolean-expr)
  if (!((scaled >= -32768.0F) && (scaled <= 32767.0F))) {
    return Result<int16_t>::Failure(Error::InvalidArgument);
  }
  const auto rounded = static_cast<int32_t>((scaled >= 0.0F) ? (scaled + 0.5F) : (scaled - 0.5F));
  return Result<int16_t>::Success(static_cast<int16_t>(rounded)); // in range: checked above
}

/**
 * @brief Register counts in degrees Celsius.
 * @param counts The two's complement register value.
 * @param resolution The register's resolution.
 * @return `counts / resolution` in °C.
 */
constexpr float ToCelsius(int16_t counts, Resolution resolution) noexcept {
  return static_cast<float>(counts) / static_cast<float>(resolution.counts_per_degree_c);
}

namespace detail {
/// Last enumerator of `Mode` (detail::IsEnumerator).
template <>
struct EnumLast<Mode> {
  static constexpr Mode kValue = Mode::Continuous; ///< The last enumerator.
};
/// Last enumerator of `OutputDataRate`.
template <>
struct EnumLast<OutputDataRate> {
  static constexpr OutputDataRate kValue = OutputDataRate::Hz16; ///< The last enumerator.
};
/// Last enumerator of `Averaging`.
template <>
struct EnumLast<Averaging> {
  static constexpr Averaging kValue = Averaging::X8; ///< The last enumerator.
};
/// Last enumerator of `AlertPolarity`.
template <>
struct EnumLast<AlertPolarity> {
  static constexpr AlertPolarity kValue = AlertPolarity::ActiveHigh; ///< The last enumerator.
};
} // namespace detail

/**
 * @brief Check a configuration without touching the chip.
 *
 * Fails when an enum holds a value that is not an enumerator, a threshold or the offset is out of
 * range, the low threshold is not below the high one, the timeout is zero or above
 * `kConversionTimeoutMaxMs`, or averaging makes one result take longer than the period of the
 * data rate in continuous mode (16 Hz allows at most 4 samples).
 *
 * @param config The configuration.
 * @return `Error::Ok` or `Error::InvalidArgument`.
 */
constexpr Error Validate(const Config& config) noexcept {
  if (!detail::IsEnumerator(config.mode) || !detail::IsEnumerator(config.odr) ||
      !detail::IsEnumerator(config.averaging) || !detail::IsEnumerator(config.alert_polarity)) {
    return Error::InvalidArgument;
  }
  // Written as "not in range" on purpose: a NaN fails every comparison, so it is rejected here.
  // NOLINTNEXTLINE(readability-simplify-boolean-expr)
  if (!((config.alert_low_c >= kAlertMinC) && (config.alert_high_c <= kAlertMaxC) &&
        (config.alert_low_c < config.alert_high_c))) {
    return Error::InvalidArgument;
  }
  // NOLINTNEXTLINE(readability-simplify-boolean-expr): "not in range" rejects NaN too
  if (!((config.offset_c >= kOffsetMinC) && (config.offset_c <= kOffsetMaxC))) {
    return Error::InvalidArgument;
  }
  if ((config.conversion_timeout_ms == 0U) ||
      (config.conversion_timeout_ms > kConversionTimeoutMaxMs)) {
    return Error::InvalidArgument;
  }
  if ((config.mode == Mode::Continuous) &&
      ((SamplesPerResult(config.averaging) * kConversionTimeMaxUs) > PeriodUs(config.odr))) {
    return Error::InvalidArgument;
  }
  return Error::Ok;
}

// Every preset is valid, proved at build time (KB-CPP20-012).
static_assert(Validate(Config{}) == Error::Ok);
static_assert(Validate(LowPowerPreset()) == Error::Ok);
static_assert(Validate(HighAccuracyPreset()) == Error::Ok);
static_assert(Validate(OneShotPreset()) == Error::Ok);
// The reset values of the threshold registers are the default thresholds (section 7.7).
static_assert(ToCounts(125.0F, kTemperatureResolution).value == 0x3E80);
static_assert(ToCounts(-40.0F, kTemperatureResolution).value == -5120); // 0xEC00
static_assert(ToCelsius(int16_t{0x3E80}, kTemperatureResolution) == 125.0F);
static_assert(!ToCounts(256.0F, kTemperatureResolution).ok());

/**
 * @brief Reached only when a `CheckedConfig` is built from an invalid configuration: it is never
 * defined, so the build fails and names it (KB-CPP20-013).
 */
void ConfigIsInvalid() noexcept;

/**
 * @brief A configuration proven valid at build time.
 *
 * `constexpr CheckedConfig kBoard{Config{.odr = OutputDataRate::Hz8}};` fails to compile, naming
 * `ConfigIsInvalid`, when `Validate()` rejects the configuration. Pass `kBoard.Get()` to the
 * driver; the driver still validates at start-up, for configurations built at run time.
 */
class CheckedConfig {
public:
  /**
   * @brief Check `config` at build time.
   * @param config The configuration; it must pass `Validate()`.
   */
  consteval explicit CheckedConfig(const Config& config) noexcept : config_(config) {
    if (Validate(config) != Error::Ok) {
      ConfigIsInvalid();
    }
  }
  /** @return The checked configuration. */
  [[nodiscard]] constexpr const Config& Get() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary: the reference would dangle (KB-CPP20-044). */
  void Get() const&& = delete;

private:
  Config config_;
};

/** @return The driver version, e.g. `"0.1.0"`, from the generated `xmpl100_version.h`. No I/O. */
constexpr const char* GetDriverVersion() noexcept {
  return XMPL100_VERSION_STRING;
}
/** @return The major version. No I/O. */
constexpr uint32_t GetDriverVersionMajor() noexcept {
  return XMPL100_VERSION_MAJOR;
}
/** @return The minor version. No I/O. */
constexpr uint32_t GetDriverVersionMinor() noexcept {
  return XMPL100_VERSION_MINOR;
}
/** @return The patch version. No I/O. */
constexpr uint32_t GetDriverVersionPatch() noexcept {
  return XMPL100_VERSION_PATCH;
}

/** @} */

} // namespace xmpl100

#endif // XMPL100_TYPES_HPP
