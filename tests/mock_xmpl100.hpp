/**
 * @file mock_xmpl100.hpp
 * @brief A register model of the XMPL100 for the host tests, plus the mock ports in
 * mock_xmpl100_bus.hpp.
 *
 * The model behaves like the datasheet: reset values, a self-clearing SOFT_RESET that holds
 * STATUS.BUSY for a few STATUS reads, one-shot conversions that finish after a few STATUS reads and
 * return to shutdown, DRDY cleared by reading TEMP_LSB, POR cleared by reading STATUS, the CRC-8
 * byte when CRC_EN is set, and OFFSET added to every result. Faults are injected through the public
 * fields.
 *
 * The model is written from the datasheet on its own (handbook KB-TST-030): it does not include the
 * driver's `xmpl100_registers.hpp` or `xmpl100_types.hpp`, but carries its own register map, reset
 * values, timing and CRC below (namespace `chip`, each with its datasheet section), so a wrong
 * constant in the driver cannot hide behind the same wrong constant in the model.
 * tests/test_model.cpp checks the two maps against each other at compile time.
 */
#ifndef XMPL100_MOCK_XMPL100_HPP
#define XMPL100_MOCK_XMPL100_HPP

#include <cstddef>
#include <cstdint>

namespace xmpl100_test {

/// The chip as the datasheet describes it, independent of the driver's headers.
namespace chip {
// Register map (section 7.1, Table 7-1): 8-bit registers, 7-bit addresses.
inline constexpr uint8_t kChipId = 0x00;
inline constexpr uint8_t kRevId = 0x01;
inline constexpr uint8_t kStatus = 0x02;
inline constexpr uint8_t kCtrl = 0x03;
inline constexpr uint8_t kConfig = 0x04;
inline constexpr uint8_t kTempMsb = 0x05;
inline constexpr uint8_t kTempLsb = 0x06;
inline constexpr uint8_t kTempCrc = 0x07;
inline constexpr uint8_t kAlertHighMsb = 0x08; // LSB at 0x09
inline constexpr uint8_t kAlertLowMsb = 0x0A;  // LSB at 0x0B
inline constexpr uint8_t kOffset = 0x0C;
inline constexpr uint8_t kLastRegister = 0x0C;
// STATUS bits (section 7.3).
inline constexpr uint8_t kStatusDrdy = 0x01;
inline constexpr uint8_t kStatusBusy = 0x02;
inline constexpr uint8_t kStatusAlertHi = 0x04;
inline constexpr uint8_t kStatusAlertLo = 0x08;
inline constexpr uint8_t kStatusPor = 0x40; // cleared by reading STATUS
inline constexpr uint8_t kStatusFault = 0x80;
// CTRL bits (section 7.4).
inline constexpr uint8_t kCtrlModeMask = 0x03; // 0 shutdown, 1 one-shot, 2 continuous
inline constexpr uint8_t kCtrlAlertEn = 0x04;
inline constexpr uint8_t kCtrlAlertPol = 0x08;
inline constexpr uint8_t kCtrlStart = 0x10;     // self-clearing
inline constexpr uint8_t kCtrlSoftReset = 0x80; // self-clearing
// CONFIG bits (section 7.5).
inline constexpr uint8_t kConfigOdrMask = 0x07;
inline constexpr uint8_t kConfigAvgShift = 3;
inline constexpr uint8_t kConfigAvgMask = 0x18;
inline constexpr uint8_t kConfigCrcEn = 0x20; // revision B only
// Reset values (section 7.1, Table 7-1).
inline constexpr uint8_t kCtrlReset = 0x02;
inline constexpr uint8_t kConfigReset = 0x02;
inline constexpr uint16_t kAlertHighReset = 0x3E80; // +125 °C
inline constexpr uint16_t kAlertLowReset = 0xEC00;  // -40 °C
// Identity (section 7.2).
inline constexpr uint8_t kChipIdValue = 0xA1;
inline constexpr uint8_t kRevisionA = 0x10;
inline constexpr uint8_t kRevisionB = 0x20;
// Timing (section 5, Table 5-2): t_RST, the longest a reset holds BUSY.
inline constexpr uint32_t kResetTimeMaxUs = 1000;

/// CRC-8 of TEMP_MSB, TEMP_LSB (section 7.6): polynomial 0x31, initial value 0xFF, no reflection,
/// no final XOR. Written out here, not taken from the driver.
constexpr uint8_t Crc8(const uint8_t* data, std::size_t len) noexcept {
  uint32_t crc = 0xFFU;
  for (std::size_t i = 0; i < len; ++i) {
    crc ^= static_cast<uint32_t>(data[i]);
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80U) != 0U ? ((crc << 1U) ^ 0x31U) & 0xFFU : (crc << 1U) & 0xFFU;
    }
  }
  return static_cast<uint8_t>(crc);
}
} // namespace chip

class Xmpl100Model {
public:
  // --- What the chip is
  // ------------------------------------------------------------------------------------
  uint8_t chip_id = chip::kChipIdValue;  ///< CHIP_ID; change it to model a wrong part.
  uint8_t rev_id = chip::kRevisionB;     ///< REV_ID.
  int16_t temperature_counts = 25 * 128; ///< The die temperature the sensor sees, in counts.

  // --- Faults to inject
  // ------------------------------------------------------------------------------------
  bool fault = false;            ///< STATUS.FAULT.
  bool corrupt_crc = false;      ///< TEMP_CRC reads wrong.
  bool reset_stuck = false;      ///< BUSY never clears after a reset.
  bool conversion_stuck = false; ///< A one-shot never finishes.
  int reset_busy_reads = 2;      ///< STATUS reads that still show BUSY after a reset.
  int conversion_busy_reads = 3; ///< STATUS reads before a one-shot result is ready.

  Xmpl100Model() noexcept {
    PowerOn();
  }

  /// Power-on: every register at its reset value, POR set.
  void PowerOn() noexcept {
    for (auto& r : regs_) {
      r = 0;
    }
    regs_[chip::kCtrl] = chip::kCtrlReset;
    regs_[chip::kConfig] = chip::kConfigReset;
    regs_[chip::kAlertHighMsb] = static_cast<uint8_t>(uint32_t{chip::kAlertHighReset} >> 8U);
    regs_[chip::kAlertHighMsb + 1] = static_cast<uint8_t>(uint32_t{chip::kAlertHighReset} & 0xFFU);
    regs_[chip::kAlertLowMsb] = static_cast<uint8_t>(uint32_t{chip::kAlertLowReset} >> 8U);
    regs_[chip::kAlertLowMsb + 1] = static_cast<uint8_t>(uint32_t{chip::kAlertLowReset} & 0xFFU);
    por_ = true;
    busy_reads_left_ = 0;
    conversion_reads_left_ = -1;
    data_ready_ = false;
    resets++;
  }

  /// RESET_N or SOFT_RESET: like power-on, and BUSY for `reset_busy_reads` STATUS reads.
  void Reset() noexcept {
    PowerOn();
    busy_reads_left_ = reset_busy_reads;
  }

  /// Time passes (the port's DelayUs): a reset finishes after t_RST, a conversion advances by one
  /// step per ms.
  void Elapse(uint32_t us) noexcept {
    if (us >= chip::kResetTimeMaxUs && !reset_stuck) {
      busy_reads_left_ = 0;
    }
    if (us >= 1000 && conversion_reads_left_ > 0 && !conversion_stuck) {
      conversion_reads_left_--;
    }
  }

  uint8_t Read(uint8_t address) noexcept {
    switch (address) {
    case chip::kChipId:
      return chip_id;
    case chip::kRevId:
      return rev_id;
    case chip::kStatus:
      return readStatus();
    case chip::kTempMsb:
      latchResultIfContinuous();
      return static_cast<uint8_t>(static_cast<uint32_t>(static_cast<uint16_t>(latched_)) >> 8U);
    case chip::kTempLsb:
      data_ready_ = false;
      return static_cast<uint8_t>(static_cast<uint32_t>(static_cast<uint16_t>(latched_)) & 0xFFU);
    case chip::kTempCrc: {
      if ((regs_[chip::kConfig] & chip::kConfigCrcEn) == 0) {
        return 0;
      }
      const uint8_t bytes[2] = {
          static_cast<uint8_t>(static_cast<uint32_t>(static_cast<uint16_t>(latched_)) >> 8U),
          static_cast<uint8_t>(static_cast<uint32_t>(static_cast<uint16_t>(latched_)) & 0xFFU)};
      const uint8_t crc = chip::Crc8(bytes, 2);
      return corrupt_crc ? static_cast<uint8_t>(static_cast<uint32_t>(crc) ^ 0xFFU) : crc;
    }
    default:
      return address <= chip::kLastRegister ? regs_[address] : 0;
    }
  }

  void Write(uint8_t address, uint8_t value) noexcept {
    writes++;
    if (address == chip::kCtrl) {
      if ((value & chip::kCtrlSoftReset) != 0) {
        Reset();
        return;
      }
      regs_[chip::kCtrl] =
          static_cast<uint8_t>(static_cast<uint32_t>(value) & ~uint32_t{chip::kCtrlStart});
      if ((value & chip::kCtrlStart) != 0 && (value & chip::kCtrlModeMask) == 1) {
        conversion_reads_left_ = conversion_busy_reads;
        data_ready_ = false; // START clears DRDY (datasheet 7.4)
        one_shots++;
      }
      return;
    }
    // Read-only registers ignore writes.
    if (address == chip::kChipId || address == chip::kRevId || address == chip::kStatus ||
        (address >= chip::kTempMsb && address <= chip::kTempCrc) || address > chip::kLastRegister) {
      return;
    }
    regs_[address] = value;
  }

  /// The raw register file, for checks.
  [[nodiscard]] uint8_t Register(uint8_t address) const noexcept {
    return regs_[address];
  }

  /// Number of resets (power-on included) and register writes so far.
  int resets = 0;
  int writes = 0;
  int one_shots = 0;

private:
  uint8_t readStatus() noexcept {
    uint8_t s = 0;
    bool busy = false;
    if (reset_stuck && busy_reads_left_ > 0) {
      busy = true;
    } else if (busy_reads_left_ > 0) {
      busy = true;
      busy_reads_left_--;
    } else {
      // No reset in progress.
    }
    if (conversion_reads_left_ > 0 || (conversion_stuck && conversion_reads_left_ == 0)) {
      busy = true;
      if (!conversion_stuck) {
        conversion_reads_left_--;
      }
    } else if (conversion_reads_left_ == 0) {
      // One-shot finished: latch the result, set DRDY, go back to shutdown (CTRL.MODE = 0).
      latched_ = measured();
      data_ready_ = true;
      conversion_reads_left_ = -1;
      regs_[chip::kCtrl] = static_cast<uint8_t>(static_cast<uint32_t>(regs_[chip::kCtrl]) &
                                                ~uint32_t{chip::kCtrlModeMask});
    } else {
      // No one-shot conversion pending.
    }
    if ((regs_[chip::kCtrl] & chip::kCtrlModeMask) == 2 && !busy) {
      data_ready_ = true; // continuous: a result is always available
    }
    if (busy) {
      s = static_cast<uint8_t>(s | chip::kStatusBusy);
    }
    if (data_ready_) {
      s = static_cast<uint8_t>(s | chip::kStatusDrdy);
    }
    if (por_) {
      s = static_cast<uint8_t>(s | chip::kStatusPor);
      por_ = false;
    }
    if (fault) {
      s = static_cast<uint8_t>(s | chip::kStatusFault);
    }
    return s;
  }

  void latchResultIfContinuous() noexcept {
    if ((regs_[chip::kCtrl] & chip::kCtrlModeMask) == 2) {
      latched_ = measured();
    }
  }

  [[nodiscard]] int16_t measured() const noexcept {
    const auto offset = static_cast<int8_t>(regs_[chip::kOffset]);
    // OFFSET counts are 1/16 °C, result counts 1/128 °C: 8 result counts per offset count.
    return static_cast<int16_t>(temperature_counts + (offset * 8));
  }

  uint8_t regs_[chip::kLastRegister + 1] = {};
  bool por_ = true;
  bool data_ready_ = false;
  int busy_reads_left_ = 0;
  int conversion_reads_left_ = -1;
  int16_t latched_ = 0;
};

} // namespace xmpl100_test

#include "mock_xmpl100_bus.hpp"

#endif // XMPL100_MOCK_XMPL100_HPP
