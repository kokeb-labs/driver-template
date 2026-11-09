/**
 * @file xmpl100_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the `CONFIG_XMPL100_*` Kconfig options.
 * @copyright Copyright (c) 2026 The driver-template authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Include this header only where you want menuconfig to choose the defaults; the driver never
 * includes it and no driver signature depends on it. Under ESP-IDF the values come from
 * `sdkconfig.h`. Without Kconfig every option falls back to the datasheet default, so
 * `KconfigDefaults()` equals `Config{}`.
 *
 * Kconfig writes a bool option set to "n" as an absent macro, so an absent
 * `CONFIG_XMPL100_CRC_ENABLE` means off in both cases.
 *
 * One configuration per program: the inline function below depends on the `CONFIG_` macros, so
 * every translation unit of one program must see the same values (ESP-IDF has one `sdkconfig.h`
 * per build; each host test is its own program). No driver or library header includes this one
 * (handbook KB-CPP20-018).
 */
#ifndef XMPL100_KCONFIG_HPP
#define XMPL100_KCONFIG_HPP

#ifdef __has_include
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "xmpl100_types.hpp"

#ifndef CONFIG_XMPL100_AVERAGING_SAMPLES
/** Samples averaged per result when Kconfig does not set it: 1, the reset value. */
#define CONFIG_XMPL100_AVERAGING_SAMPLES 1
#endif
#ifndef CONFIG_XMPL100_CONVERSION_TIMEOUT_MS
/** One-shot timeout in milliseconds when Kconfig does not set it: `Config{}.conversion_timeout_ms`.
 */
#define CONFIG_XMPL100_CONVERSION_TIMEOUT_MS 100
#endif

namespace xmpl100 {

/**
 * @brief The configuration menuconfig selected, on top of the datasheet defaults.
 * @return `Config{}` with the output data rate, averaging, CRC and timeout taken from Kconfig.
 */
constexpr Config KconfigDefaults() noexcept {
  Config c{};
#ifdef CONFIG_XMPL100_ODR_1HZ
  c.odr = OutputDataRate::Hz1;
#elif defined(CONFIG_XMPL100_ODR_2HZ)
  c.odr = OutputDataRate::Hz2;
#elif defined(CONFIG_XMPL100_ODR_8HZ)
  c.odr = OutputDataRate::Hz8;
#elif defined(CONFIG_XMPL100_ODR_16HZ)
  c.odr = OutputDataRate::Hz16;
#else
  c.odr = OutputDataRate::Hz4;
#endif
  constexpr int kSamples = CONFIG_XMPL100_AVERAGING_SAMPLES;
  if constexpr (kSamples >= 8) {
    c.averaging = Averaging::X8;
  } else if constexpr (kSamples >= 4) {
    c.averaging = Averaging::X4;
  } else if constexpr (kSamples >= 2) {
    c.averaging = Averaging::X2;
  } else {
    c.averaging = Averaging::X1;
  }
#if defined(CONFIG_XMPL100_CRC_ENABLE) && CONFIG_XMPL100_CRC_ENABLE
  c.crc_enable = true;
#endif
  c.conversion_timeout_ms = CONFIG_XMPL100_CONVERSION_TIMEOUT_MS;
  return c;
}

} // namespace xmpl100

#endif // XMPL100_KCONFIG_HPP
