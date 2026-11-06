/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): the defaults without Kconfig, and menuconfig values when they are set.
 *
 * Built twice: without CONFIG_ macros (KCONFIG_SET undefined) and with the values an sdkconfig.h
 * would define (KCONFIG_SET=1, see tests/CMakeLists.txt).
 */
#include "test_harness.hpp"
#include "xmpl100_kconfig.hpp"

#ifdef KCONFIG_SET
TEST_CASE("Kconfig values reach the Config") {
  constexpr xmpl100::Config kC = xmpl100::KconfigDefaults();
  CHECK_EQ(kC.odr, xmpl100::OutputDataRate::Hz8);
  CHECK_EQ(kC.averaging, xmpl100::Averaging::X4);
  CHECK(kC.crc_enable);
  CHECK_EQ(kC.conversion_timeout_ms, 250U);
  CHECK_EQ(kC.mode, xmpl100::Mode::Continuous); // not a Kconfig option: datasheet default
  CHECK_EQ(xmpl100::Validate(kC), xmpl100::Error::Ok);
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() equals Config{}") {
  constexpr xmpl100::Config kC = xmpl100::KconfigDefaults();
  constexpr xmpl100::Config kD{};
  CHECK_EQ(kC.odr, kD.odr);
  CHECK_EQ(kC.averaging, kD.averaging);
  CHECK_EQ(kC.crc_enable, kD.crc_enable);
  CHECK_EQ(kC.conversion_timeout_ms, kD.conversion_timeout_ms);
}
#endif
