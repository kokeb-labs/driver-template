/**
 * @file test_model.cpp
 * @brief The chip model's register map against the driver's, at compile time (handbook KB-TST-030).
 *
 * tests/mock_xmpl100.hpp is written from the datasheet without the driver's headers, so a constant
 * the driver gets wrong is not copied into the model. These checks make the two agree, or the build
 * fails: a difference is a mistake in one of them, found against the datasheet section the model
 * names.
 */
#include <array>
#include <cstdint>

#include "mock_xmpl100.hpp"
#include "test_harness.hpp"
#include "xmpl100.hpp"

namespace {

namespace chip = xmpl100_test::chip;
namespace reg = xmpl100::reg;

// --8<-- [start:cross-check]
// Register map (section 7.1).
static_assert(chip::kChipId == reg::kChipId && chip::kRevId == reg::kRevId &&
              chip::kStatus == reg::kStatus && chip::kCtrl == reg::kCtrl &&
              chip::kConfig == reg::kConfig);
static_assert(chip::kTempMsb == reg::kTempMsb && chip::kTempLsb == reg::kTempLsb &&
              chip::kTempCrc == reg::kTempCrc);
static_assert(chip::kAlertHighMsb == reg::kAlertHighMsb &&
              chip::kAlertLowMsb == reg::kAlertLowMsb && chip::kOffset == reg::kOffset &&
              chip::kLastRegister == reg::kLastRegister);
// STATUS, CTRL and CONFIG fields (sections 7.3 to 7.5).
static_assert(chip::kStatusDrdy == reg::kStatusDrdy && chip::kStatusBusy == reg::kStatusBusy &&
              chip::kStatusAlertHi == reg::kStatusAlertHi &&
              chip::kStatusAlertLo == reg::kStatusAlertLo && chip::kStatusPor == reg::kStatusPor &&
              chip::kStatusFault == reg::kStatusFault);
static_assert(chip::kCtrlModeMask == reg::kCtrlModeMask &&
              chip::kCtrlAlertEn == reg::kCtrlAlertEn &&
              chip::kCtrlAlertPol == reg::kCtrlAlertPol && chip::kCtrlStart == reg::kCtrlStart &&
              chip::kCtrlSoftReset == reg::kCtrlSoftReset);
static_assert(chip::kConfigOdrMask == reg::kConfigOdrMask &&
              chip::kConfigAvgShift == reg::kConfigAvgShift &&
              chip::kConfigAvgMask == reg::kConfigAvgMask &&
              chip::kConfigCrcEn == reg::kConfigCrcEn);
// Reset values, identity and timing (sections 5, 7.1, 7.2).
static_assert(chip::kCtrlReset == reg::kCtrlReset && chip::kConfigReset == reg::kConfigReset &&
              chip::kAlertHighReset == reg::kAlertHighReset &&
              chip::kAlertLowReset == reg::kAlertLowReset);
static_assert(chip::kChipIdValue == xmpl100::kChipId && chip::kRevisionA == xmpl100::kRevisionA &&
              chip::kRevisionB == xmpl100::kRevisionB);
static_assert(chip::kResetTimeMaxUs == xmpl100::kResetTimeMaxUs);
// The clear-on-read bits the driver latches are the ones the model clears on a STATUS read.
static_assert(reg::kStatusClearOnRead == chip::kStatusPor);
// --8<-- [end:cross-check]

// The two CRC-8 implementations agree on the datasheet's example and on edge values.
constexpr std::array<uint8_t, 2> kZero = {0x00, 0x00};
constexpr std::array<uint8_t, 2> kOnes = {0xFF, 0xFF};
constexpr std::array<uint8_t, 2> kSample = {0x0C, 0x80}; // +25 °C
static_assert(chip::Crc8(kZero.data(), kZero.size()) == reg::Crc8(kZero.data(), kZero.size()));
static_assert(chip::Crc8(kOnes.data(), kOnes.size()) == reg::Crc8(kOnes.data(), kOnes.size()));
static_assert(chip::Crc8(kSample.data(), kSample.size()) ==
              reg::Crc8(kSample.data(), kSample.size()));

} // namespace

TEST_CASE("The model's CRC-8 matches the driver's for every first byte") {
  for (uint32_t b = 0; b <= 0xFFU; ++b) {
    const std::array<uint8_t, 2> bytes = {static_cast<uint8_t>(b), static_cast<uint8_t>(0xFFU - b)};
    CHECK_EQ(chip::Crc8(bytes.data(), bytes.size()), reg::Crc8(bytes.data(), bytes.size()));
  }
}
