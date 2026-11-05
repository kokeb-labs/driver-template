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
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "xmpl100_registers.hpp"
#include "xmpl100_types.hpp"

namespace xmpl100_test {

namespace reg = xmpl100::reg;

class Xmpl100Model {
public:
  // --- What the chip is
  // ------------------------------------------------------------------------------------
  uint8_t chip_id = xmpl100::kChipId;    ///< CHIP_ID; change it to model a wrong part.
  uint8_t rev_id = xmpl100::kRevisionB;  ///< REV_ID.
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
    regs_[reg::kCtrl] = reg::kCtrlReset;
    regs_[reg::kConfig] = reg::kConfigReset;
    regs_[reg::kAlertHighMsb] = static_cast<uint8_t>(reg::kAlertHighReset >> 8);
    regs_[reg::kAlertHighMsb + 1] = static_cast<uint8_t>(reg::kAlertHighReset & 0xFF);
    regs_[reg::kAlertLowMsb] = static_cast<uint8_t>(reg::kAlertLowReset >> 8);
    regs_[reg::kAlertLowMsb + 1] = static_cast<uint8_t>(reg::kAlertLowReset & 0xFF);
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
    if (us >= xmpl100::kResetTimeMaxUs && !reset_stuck) {
      busy_reads_left_ = 0;
    }
    if (us >= 1000 && conversion_reads_left_ > 0 && !conversion_stuck) {
      conversion_reads_left_--;
    }
  }

  uint8_t Read(uint8_t address) noexcept {
    switch (address) {
    case reg::kChipId:
      return chip_id;
    case reg::kRevId:
      return rev_id;
    case reg::kStatus:
      return ReadStatus();
    case reg::kTempMsb:
      LatchResultIfContinuous();
      return static_cast<uint8_t>(static_cast<uint16_t>(latched_) >> 8);
    case reg::kTempLsb:
      data_ready_ = false;
      return static_cast<uint8_t>(static_cast<uint16_t>(latched_) & 0xFF);
    case reg::kTempCrc: {
      if ((regs_[reg::kConfig] & reg::kConfigCrcEn) == 0) {
        return 0;
      }
      const uint8_t bytes[2] = {static_cast<uint8_t>(static_cast<uint16_t>(latched_) >> 8),
                                static_cast<uint8_t>(static_cast<uint16_t>(latched_) & 0xFF)};
      const uint8_t crc = reg::Crc8(bytes, 2);
      return corrupt_crc ? static_cast<uint8_t>(crc ^ 0xFF) : crc;
    }
    default:
      return address <= reg::kLastRegister ? regs_[address] : 0;
    }
  }

  void Write(uint8_t address, uint8_t value) noexcept {
    writes++;
    if (address == reg::kCtrl) {
      if ((value & reg::kCtrlSoftReset) != 0) {
        Reset();
        return;
      }
      regs_[reg::kCtrl] = static_cast<uint8_t>(value & ~reg::kCtrlStart);
      if ((value & reg::kCtrlStart) != 0 && (value & reg::kCtrlModeMask) == 1) {
        conversion_reads_left_ = conversion_busy_reads;
        data_ready_ = false; // START clears DRDY (datasheet 7.4)
        one_shots++;
      }
      return;
    }
    // Read-only registers ignore writes.
    if (address == reg::kChipId || address == reg::kRevId || address == reg::kStatus ||
        (address >= reg::kTempMsb && address <= reg::kTempCrc) || address > reg::kLastRegister) {
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
  uint8_t ReadStatus() noexcept {
    uint8_t s = 0;
    bool busy = false;
    if (reset_stuck && busy_reads_left_ > 0) {
      busy = true;
    } else if (busy_reads_left_ > 0) {
      busy = true;
      busy_reads_left_--;
    }
    if (conversion_reads_left_ > 0 || (conversion_stuck && conversion_reads_left_ == 0)) {
      busy = true;
      if (!conversion_stuck) {
        conversion_reads_left_--;
      }
    } else if (conversion_reads_left_ == 0) {
      // One-shot finished: latch the result, set DRDY, go back to shutdown (CTRL.MODE = 0).
      latched_ = Measured();
      data_ready_ = true;
      conversion_reads_left_ = -1;
      regs_[reg::kCtrl] = static_cast<uint8_t>(regs_[reg::kCtrl] & ~reg::kCtrlModeMask);
    }
    if ((regs_[reg::kCtrl] & reg::kCtrlModeMask) == 2 && !busy) {
      data_ready_ = true; // continuous: a result is always available
    }
    if (busy) {
      s = static_cast<uint8_t>(s | reg::kStatusBusy);
    }
    if (data_ready_) {
      s = static_cast<uint8_t>(s | reg::kStatusDrdy);
    }
    if (por_) {
      s = static_cast<uint8_t>(s | reg::kStatusPor);
      por_ = false;
    }
    if (fault) {
      s = static_cast<uint8_t>(s | reg::kStatusFault);
    }
    return s;
  }

  void LatchResultIfContinuous() noexcept {
    if ((regs_[reg::kCtrl] & reg::kCtrlModeMask) == 2) {
      latched_ = Measured();
    }
  }

  [[nodiscard]] int16_t Measured() const noexcept {
    const auto offset = static_cast<int8_t>(regs_[reg::kOffset]);
    // OFFSET counts are 1/16 °C, result counts 1/128 °C: 8 result counts per offset count.
    return static_cast<int16_t>(temperature_counts + offset * 8);
  }

  uint8_t regs_[reg::kLastRegister + 1] = {};
  bool por_ = true;
  bool data_ready_ = false;
  int busy_reads_left_ = 0;
  int conversion_reads_left_ = -1;
  int16_t latched_ = 0;
};

} // namespace xmpl100_test

#include "mock_xmpl100_bus.hpp"
