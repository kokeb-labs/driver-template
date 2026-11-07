/**
 * @file xmpl100.ipp
 * @brief XMPL100 driver implementation, included by `xmpl100.hpp`.
 * @copyright Copyright (c) 2026 The Kokeb Labs Authors. SPDX-License-Identifier: MIT
 *
 * Section numbers refer to the XMPL100 datasheet.
 */
#pragma once

namespace xmpl100 {

// -------------------------------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> XMPL100<Port>::EnsureInitialized() noexcept {
  if (initialized_) {
    return Result<void>::Success();
  }
  if (auto r = initialize(); !r) {
    return r;
  }
  initialized_ = true;
  detail::Log(bus_.GetPort(), LogLevel::Info, "xmpl100: initialised");
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> XMPL100<Port>::initialize() noexcept {
  const Config config = config_;
  if (Validate(config) != Error::Ok) {
    return fail(Error::InvalidArgument, "xmpl100: invalid Config");
  }
  if (!bus_.GetPort().EnsureInitialized()) {
    return fail(Error::Bus, "xmpl100: the port could not bring the bus up");
  }
  // Hardware reset when the port drives RESET_N, else a soft reset.
  if constexpr (detail::HasGpioSet<Port>) {
    if (auto r = pulseResetPin(); !r) {
      return r;
    }
  }
  return resetAndConfigure(config, !detail::HasGpioSet<Port>);
}

template <SpiPort Port>
Result<void> XMPL100<Port>::Deinitialize() noexcept {
  if (!initialized_) {
    return Result<void>::Success();
  }
  initialized_ = false;
  Config shutdown = config_;
  shutdown.mode = Mode::Shutdown;
  return writeRegister(reg::kCtrl, ctrlValue(shutdown, false));
}

template <SpiPort Port>
Result<void> XMPL100<Port>::SoftReset() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  initialized_ = false;
  if (auto r = resetAndConfigure(config_, true); !r) {
    return r;
  }
  initialized_ = true;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> XMPL100<Port>::HardwareReset() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if constexpr (!detail::HasGpioSet<Port>) {
    return Result<void>::Failure(Error::Unsupported);
  } else {
    initialized_ = false;
    if (auto r = pulseResetPin(); !r) {
      return r;
    }
    if (auto r = resetAndConfigure(config_, false); !r) {
      return r;
    }
    initialized_ = true;
    return Result<void>::Success();
  }
}

// -------------------------------------------------------------------------------------------------
// Configuration
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> XMPL100<Port>::ApplyConfig(const Config& config) noexcept {
  if (Validate(config) != Error::Ok) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  if (!initialized_) {
    config_ = config; // applied by the first EnsureInitialized()
    return Result<void>::Success();
  }
  return writeConfigRegisters(config);
}

template <SpiPort Port>
Result<void> XMPL100<Port>::SetMode(Mode mode) noexcept {
  Config next = config_;
  next.mode = mode;
  return ApplyConfig(next);
}

template <SpiPort Port>
Result<void> XMPL100<Port>::SetAlertThresholdsC(float low_c, float high_c) noexcept {
  Config next = config_;
  next.alert_low_c = low_c;
  next.alert_high_c = high_c;
  return ApplyConfig(next);
}

// -------------------------------------------------------------------------------------------------
// Measurement
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<Status> XMPL100<Port>::ReadStatus() noexcept {
  if (!initialized_) {
    return Result<Status>::Failure(Error::NotInitialized);
  }
  const auto raw = readRegister(reg::kStatus);
  if (!raw) {
    return Result<Status>::Failure(raw.error);
  }
  Status s{};
  s.data_ready = (raw.value & reg::kStatusDrdy) != 0;
  s.busy = (raw.value & reg::kStatusBusy) != 0;
  s.alert_high = (raw.value & reg::kStatusAlertHi) != 0;
  s.alert_low = (raw.value & reg::kStatusAlertLo) != 0;
  s.power_on_reset = (raw.value & reg::kStatusPor) != 0;
  s.fault = (raw.value & reg::kStatusFault) != 0;
  return Result<Status>::Success(s);
}

template <SpiPort Port>
Result<int16_t> XMPL100<Port>::ReadRawTemperature() noexcept {
  const auto status = ReadStatus();
  if (!status) {
    return Result<int16_t>::Failure(status.error);
  }
  if (status.value.fault) {
    return Result<int16_t>::Failure(Error::DeviceFault);
  }
  if (status.value.power_on_reset) {
    // The chip lost its configuration (brown-out): the result is from the reset defaults.
    initialized_ = false;
    detail::Log(bus_.GetPort(), LogLevel::Warning, "xmpl100: unexpected reset");
    return Result<int16_t>::Failure(Error::UnexpectedReset);
  }
  // TEMP_MSB, TEMP_LSB and TEMP_CRC in one burst: the chip latches the pair on the first byte
  // (section 7.6).
  std::array<uint8_t, 3> bytes{};
  const std::size_t len = config_.crc_enable ? 3 : 2;
  if (const Error e = bus_.Read(reg::kTempMsb, bytes.data(), len); e != Error::Ok) {
    return Result<int16_t>::Failure(e);
  }
  if (config_.crc_enable && reg::Crc8(bytes.data(), 2) != bytes[2]) {
    return Result<int16_t>::Failure(Error::Crc);
  }
  const auto counts = static_cast<int16_t>(static_cast<uint16_t>((bytes[0] << 8) | bytes[1]));
  return Result<int16_t>::Success(counts);
}

template <SpiPort Port>
Result<float> XMPL100<Port>::ReadTemperatureC() noexcept {
  const auto raw = ReadRawTemperature();
  if (!raw) {
    return Result<float>::Failure(raw.error);
  }
  return Result<float>::Success(static_cast<float>(raw.value) /
                                static_cast<float>(kCountsPerDegreeC));
}

template <SpiPort Port>
Result<void> XMPL100<Port>::TriggerOneShot() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if (config_.mode == Mode::Continuous) {
    return Result<void>::Failure(Error::Busy);
  }
  const auto status = ReadStatus();
  if (!status) {
    return Result<void>::Failure(status.error);
  }
  if (status.value.busy) {
    return Result<void>::Failure(Error::Busy);
  }
  Config one_shot = config_;
  one_shot.mode = Mode::OneShot;
  return writeRegister(reg::kCtrl, ctrlValue(one_shot, true));
}

template <SpiPort Port>
Result<void> XMPL100<Port>::WaitDataReady() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  for (uint32_t ms = 0; ms <= config_.conversion_timeout_ms; ++ms) {
    const auto status = ReadStatus();
    if (!status) {
      return Result<void>::Failure(status.error);
    }
    if (status.value.fault) {
      return Result<void>::Failure(Error::DeviceFault);
    }
    if (status.value.data_ready) {
      return Result<void>::Success();
    }
    detail::DelayUs(bus_.GetPort(), 1000);
  }
  return Result<void>::Failure(Error::Timeout);
}

template <SpiPort Port>
Result<float> XMPL100<Port>::MeasureOnceC() noexcept {
  if (auto r = TriggerOneShot(); !r) {
    return Result<float>::Failure(r.error);
  }
  if (auto r = WaitDataReady(); !r) {
    return Result<float>::Failure(r.error);
  }
  return ReadTemperatureC();
}

// -------------------------------------------------------------------------------------------------
// Private
// -------------------------------------------------------------------------------------------------

template <SpiPort Port>
Result<void> XMPL100<Port>::pulseResetPin() noexcept {
  // RESET_N low for at least t_RST_PW, then released (section 8.3).
  Port& port = bus_.GetPort();
  if constexpr (detail::HasGpioSet<Port>) {
    if (!port.GpioSet(CtrlPin::Reset, GpioSignal::ACTIVE)) {
      return fail(Error::Bus, "xmpl100: RESET_N could not be asserted");
    }
    detail::DelayUs(port, kResetPulseMinUs);
    if (!port.GpioSet(CtrlPin::Reset, GpioSignal::INACTIVE)) {
      return fail(Error::Bus, "xmpl100: RESET_N could not be released");
    }
    return waitResetDone();
  } else {
    return Result<void>::Failure(Error::Unsupported);
  }
}

template <SpiPort Port>
Result<void> XMPL100<Port>::resetAndConfigure(const Config& config, bool soft_reset) noexcept {
  // Soft reset unless RESET_N was just pulsed: the chip starts from its reset values (section 8.2).
  if (soft_reset) {
    if (auto r = writeRegister(reg::kCtrl, reg::kCtrlSoftReset); !r) {
      return fail(r.error, "xmpl100: soft reset write failed");
    }
    if (auto r = waitResetDone(); !r) {
      return r;
    }
  }
  const auto id = readRegister(reg::kChipId);
  if (!id) {
    return fail(id.error, "xmpl100: CHIP_ID read failed");
  }
  if (id.value != kChipId) {
    return fail(Error::DeviceIdMismatch, "xmpl100: CHIP_ID is not 0xA1");
  }
  const auto rev = readRegister(reg::kRevId);
  if (!rev) {
    return fail(rev.error, "xmpl100: REV_ID read failed");
  }
  revision_ = rev.value;
  // Clear STATUS.POR, so that a later POR means an unexpected reset.
  if (auto st = readRegister(reg::kStatus); !st) {
    return fail(st.error, "xmpl100: STATUS read failed");
  }
  if (auto r = writeConfigRegisters(config); !r) {
    return fail(r.error, r.error == Error::Unsupported ? "xmpl100: CRC needs revision B silicon"
                                                       : "xmpl100: configuration write failed");
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> XMPL100<Port>::waitResetDone() noexcept {
  // BUSY stays set for at most t_RST after a reset (section 8.2): wait it, then confirm.
  detail::DelayUs(bus_.GetPort(), kResetTimeMaxUs);
  for (uint32_t i = 0; i < kResetPolls; ++i) {
    const auto status = readRegister(reg::kStatus);
    if (!status) {
      return fail(status.error, "xmpl100: STATUS read failed during reset");
    }
    if ((status.value & reg::kStatusBusy) == 0) {
      return Result<void>::Success();
    }
    detail::DelayUs(bus_.GetPort(), kResetTimeMaxUs);
  }
  return fail(Error::Timeout, "xmpl100: reset did not finish (STATUS.BUSY stuck)");
}

template <SpiPort Port>
Result<void> XMPL100<Port>::writeConfigRegisters(const Config& config) noexcept {
  if (config.crc_enable && revision_ < kRevisionB) {
    return Result<void>::Failure(Error::Unsupported);
  }
  // Shutdown while the thresholds and averaging change, so no result mixes old and new settings.
  Config idle = config;
  idle.mode = Mode::Shutdown;
  if (auto r = writeRegister(reg::kCtrl, ctrlValue(idle, false)); !r) {
    return r;
  }
  const auto high = static_cast<uint16_t>(toCounts(config.alert_high_c, kCountsPerDegreeC));
  const auto low = static_cast<uint16_t>(toCounts(config.alert_low_c, kCountsPerDegreeC));
  const std::array<uint8_t, 4> thresholds = {
      static_cast<uint8_t>(high >> 8), static_cast<uint8_t>(high & 0xFF),
      static_cast<uint8_t>(low >> 8), static_cast<uint8_t>(low & 0xFF)};
  if (const Error e = bus_.Write(reg::kAlertHighMsb, thresholds.data(), thresholds.size());
      e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  const auto offset = static_cast<uint8_t>(toCounts(config.offset_c, kOffsetCountsPerDegreeC));
  if (auto r = writeRegister(reg::kOffset, offset); !r) {
    return r;
  }
  if (auto r = writeRegister(reg::kConfig, configValue(config)); !r) {
    return r;
  }
  if (auto r = writeRegister(reg::kCtrl, ctrlValue(config, false)); !r) {
    return r;
  }
  config_ = config;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<uint8_t> XMPL100<Port>::readRegister(uint8_t reg) noexcept {
  uint8_t value = 0;
  if (const Error e = bus_.Read(reg, &value, 1); e != Error::Ok) {
    return Result<uint8_t>::Failure(e);
  }
  return Result<uint8_t>::Success(value);
}

template <SpiPort Port>
Result<void> XMPL100<Port>::writeRegister(uint8_t reg, uint8_t value) noexcept {
  if (const Error e = bus_.Write(reg, &value, 1); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> XMPL100<Port>::fail(Error error, const char* message) noexcept {
  detail::Log(bus_.GetPort(), LogLevel::Error, message);
  return Result<void>::Failure(error);
}

template <SpiPort Port>
uint8_t XMPL100<Port>::ctrlValue(const Config& config, bool start) noexcept {
  auto value = static_cast<uint8_t>(static_cast<uint8_t>(config.mode) & reg::kCtrlModeMask);
  if (config.alert_enable) {
    value = static_cast<uint8_t>(value | reg::kCtrlAlertEn);
  }
  if (config.alert_polarity == AlertPolarity::ActiveHigh) {
    value = static_cast<uint8_t>(value | reg::kCtrlAlertPol);
  }
  if (start) {
    value = static_cast<uint8_t>(value | reg::kCtrlStart);
  }
  return value;
}

template <SpiPort Port>
uint8_t XMPL100<Port>::configValue(const Config& config) noexcept {
  auto value = static_cast<uint8_t>(static_cast<uint8_t>(config.odr) & reg::kConfigOdrMask);
  value = static_cast<uint8_t>(
      value |
      ((static_cast<uint8_t>(config.averaging) << reg::kConfigAvgShift) & reg::kConfigAvgMask));
  if (config.crc_enable) {
    value = static_cast<uint8_t>(value | reg::kConfigCrcEn);
  }
  return value;
}

template <SpiPort Port>
int16_t XMPL100<Port>::toCounts(float celsius, int32_t counts_per_degree) noexcept {
  // Round half away from zero without <cmath>; Validate() keeps the result inside int16_t.
  const float scaled = celsius * static_cast<float>(counts_per_degree);
  const auto rounded = static_cast<int32_t>(scaled >= 0.0F ? scaled + 0.5F : scaled - 0.5F);
  return static_cast<int16_t>(rounded);
}

} // namespace xmpl100
