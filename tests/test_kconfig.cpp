/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): the defaults without Kconfig, and menuconfig values when they are set.
 *
 * Built twice: without CONFIG_ macros (KCONFIG_SET undefined) and with the values an sdkconfig.h
 * would define (KCONFIG_SET=1, see tests/CMakeLists.txt).
 */
#include "test_harness.hpp"
#include "xmpl100_kconfig.hpp"

#if defined(KCONFIG_SET)
TEST_CASE("Kconfig values reach the Config") {
  constexpr xmpl100::Config c = xmpl100::KconfigDefaults();
  CHECK_EQ(c.odr, xmpl100::OutputDataRate::Hz8);
  CHECK_EQ(c.averaging, xmpl100::Averaging::X4);
  CHECK(c.crc_enable);
  CHECK_EQ(c.conversion_timeout_ms, 250U);
  CHECK_EQ(c.mode, xmpl100::Mode::Continuous); // not a Kconfig option: datasheet default
  CHECK_EQ(xmpl100::Validate(c), xmpl100::Error::Ok);
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() equals Config{}") {
  constexpr xmpl100::Config c = xmpl100::KconfigDefaults();
  constexpr xmpl100::Config d{};
  CHECK_EQ(c.odr, d.odr);
  CHECK_EQ(c.averaging, d.averaging);
  CHECK_EQ(c.crc_enable, d.crc_enable);
  CHECK_EQ(c.conversion_timeout_ms, d.conversion_timeout_ms);
}
#endif
