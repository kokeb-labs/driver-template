/**
 * @file xmpl100_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the `CONFIG_XMPL100_*` Kconfig options.
 * @copyright Copyright (c) 2026 The Kokeb Labs Authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Include this header only where you want menuconfig to choose the defaults; the driver never
 * includes it and no driver signature depends on it. Under ESP-IDF the values come from
 * `sdkconfig.h`. Without Kconfig every option falls back to the datasheet default, so
 * `KconfigDefaults()` equals `Config{}`.
 *
 * Kconfig writes a bool option set to "n" as an absent macro, so an absent
 * `CONFIG_XMPL100_CRC_ENABLE` means off in both cases.
 */
#pragma once

#if defined(__has_include)
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "xmpl100_types.hpp"

// NOLINTBEGIN(cppcoreguidelines-macro-usage): Kconfig values are macros by definition.
#ifndef CONFIG_XMPL100_AVERAGING_SAMPLES
/** Samples averaged per result when Kconfig does not set it: 1, the reset value. */
#define CONFIG_XMPL100_AVERAGING_SAMPLES 1
#endif
#ifndef CONFIG_XMPL100_CONVERSION_TIMEOUT_MS
/** One-shot timeout in milliseconds when Kconfig does not set it: `Config{}.conversion_timeout_ms`.
 */
#define CONFIG_XMPL100_CONVERSION_TIMEOUT_MS 100
#endif
// NOLINTEND(cppcoreguidelines-macro-usage)

namespace xmpl100 {

/**
 * @brief The configuration menuconfig selected, on top of the datasheet defaults.
 * @return `Config{}` with the output data rate, averaging, CRC and timeout taken from Kconfig.
 */
constexpr Config KconfigDefaults() noexcept {
  Config c{};
#if defined(CONFIG_XMPL100_ODR_1HZ)
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
  constexpr int samples = CONFIG_XMPL100_AVERAGING_SAMPLES;
  c.averaging = samples >= 8   ? Averaging::X8
                : samples >= 4 ? Averaging::X4
                : samples >= 2 ? Averaging::X2
                               : Averaging::X1;
#if defined(CONFIG_XMPL100_CRC_ENABLE) && CONFIG_XMPL100_CRC_ENABLE
  c.crc_enable = true;
#endif
  c.conversion_timeout_ms = CONFIG_XMPL100_CONVERSION_TIMEOUT_MS;
  return c;
}

} // namespace xmpl100
