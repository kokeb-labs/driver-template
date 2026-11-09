/**
 * @file xmpl100_registers.hpp
 * @brief XMPL100 register map: addresses, bit fields and reset values (datasheet section 7).
 * @copyright Copyright (c) 2026 The Kokeb Labs Authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Registers are 8 bits wide with 7-bit addresses. Multi-byte values are big-endian (MSB at the
 * lower address) and the address auto-increments within one transfer, so a temperature read is one
 * 3-byte burst from TEMP_MSB.
 */
#ifndef XMPL100_REGISTERS_HPP
#define XMPL100_REGISTERS_HPP

#include <cstddef>
#include <cstdint>

/** @brief The XMPL100 register map. */
namespace xmpl100::reg {

/**
 * @defgroup xmpl100_registers Register map
 * @brief Addresses, fields and reset values.
 * @{
 */

inline constexpr uint8_t kChipId = 0x00;  ///< CHIP_ID (R): always 0xA1.
inline constexpr uint8_t kRevId = 0x01;   ///< REV_ID (R): 0x10 revision A, 0x20 revision B.
inline constexpr uint8_t kStatus = 0x02;  ///< STATUS (R): see the kStatus* bits. Read clears POR.
inline constexpr uint8_t kCtrl = 0x03;    ///< CTRL (R/W): mode, alert, START and SOFT_RESET.
inline constexpr uint8_t kConfig = 0x04;  ///< CONFIG (R/W): ODR, averaging, CRC enable.
inline constexpr uint8_t kTempMsb = 0x05; ///< TEMP_MSB (R): result bits 15..8, two's complement.
inline constexpr uint8_t kTempLsb = 0x06; ///< TEMP_LSB (R): result bits 7..0. Read clears DRDY.
inline constexpr uint8_t kTempCrc =
    0x07; ///< TEMP_CRC (R): CRC-8 of TEMP_MSB, TEMP_LSB; 0 when CRC_EN = 0.
inline constexpr uint8_t kAlertHighMsb = 0x08; ///< ALERT_HIGH (R/W), bits 15..8; LSB at 0x09.
inline constexpr uint8_t kAlertLowMsb = 0x0A;  ///< ALERT_LOW (R/W), bits 15..8; LSB at 0x0B.
inline constexpr uint8_t kOffset = 0x0C;       ///< OFFSET (R/W): signed, 1/16 °C per count.
inline constexpr uint8_t kLastRegister = 0x0C; ///< Highest implemented address.

inline constexpr uint8_t kStatusDrdy = 1U << 0;    ///< STATUS.DRDY: a new result is waiting.
inline constexpr uint8_t kStatusBusy = 1U << 1;    ///< STATUS.BUSY: reset or one-shot in progress.
inline constexpr uint8_t kStatusAlertHi = 1U << 2; ///< STATUS.ALERT_HI.
inline constexpr uint8_t kStatusAlertLo = 1U << 3; ///< STATUS.ALERT_LO.
inline constexpr uint8_t kStatusPor = 1U << 6;   ///< STATUS.POR: reset since the last STATUS read.
inline constexpr uint8_t kStatusFault = 1U << 7; ///< STATUS.FAULT: sensing element open or shorted.
/// STATUS bits a read clears (section 7.3): the driver ORs them into its latch on every STATUS
/// read.
inline constexpr uint8_t kStatusClearOnRead = kStatusPor;

inline constexpr uint8_t kCtrlModeMask = 0x03;     ///< CTRL.MODE, bits 1..0 (xmpl100::Mode).
inline constexpr uint8_t kCtrlAlertEn = 1U << 2;   ///< CTRL.ALERT_EN.
inline constexpr uint8_t kCtrlAlertPol = 1U << 3;  ///< CTRL.ALERT_POL: 1 = active high.
inline constexpr uint8_t kCtrlStart = 1U << 4;     ///< CTRL.START: start a one-shot; self-clearing.
inline constexpr uint8_t kCtrlSoftReset = 1U << 7; ///< CTRL.SOFT_RESET: self-clearing.

inline constexpr uint8_t kConfigOdrMask =
    0x07;                                     ///< CONFIG.ODR, bits 2..0 (xmpl100::OutputDataRate).
inline constexpr uint8_t kConfigAvgShift = 3; ///< CONFIG.AVG position.
inline constexpr uint8_t kConfigAvgMask = 0x18;  ///< CONFIG.AVG, bits 4..3 (xmpl100::Averaging).
inline constexpr uint8_t kConfigCrcEn = 1U << 5; ///< CONFIG.CRC_EN; revision B only.

inline constexpr uint8_t kCtrlReset = 0x02;   ///< CTRL after reset: continuous, alert off.
inline constexpr uint8_t kConfigReset = 0x02; ///< CONFIG after reset: 4 Hz, no averaging, no CRC.
inline constexpr uint16_t kAlertHighReset = 0x3E80; ///< ALERT_HIGH after reset: +125 °C.
inline constexpr uint16_t kAlertLowReset = 0xEC00;  ///< ALERT_LOW after reset: -40 °C.

inline constexpr uint8_t kCrcPolynomial = 0x31; ///< CRC-8 polynomial x^8 + x^5 + x^4 + 1.
inline constexpr uint8_t kCrcInit = 0xFF;       ///< CRC-8 initial value.

/**
 * @brief CRC-8 of the temperature bytes as the chip computes it (polynomial 0x31, initial value
 * 0xFF, no reflection, no final XOR).
 * @param data The bytes, TEMP_MSB first.
 * @param len Number of bytes.
 * @return The CRC.
 */
constexpr uint8_t Crc8(const uint8_t* data, std::size_t len) noexcept {
  uint8_t crc = kCrcInit;
  for (std::size_t i = 0; i < len; ++i) {
    crc = static_cast<uint8_t>(static_cast<uint32_t>(crc) ^ static_cast<uint32_t>(data[i]));
    for (int bit = 0; bit < 8; ++bit) {
      const uint32_t wide = crc; // promote as unsigned (MISRA C++:2023 Rule 7.0.5)
      crc = (wide & 0x80U) != 0 ? static_cast<uint8_t>((wide << 1U) ^ uint32_t{kCrcPolynomial})
                                : static_cast<uint8_t>(wide << 1U);
    }
  }
  return crc;
}

/** @} */

} // namespace xmpl100::reg

#endif // XMPL100_REGISTERS_HPP
