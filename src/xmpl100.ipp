/**
 * @file xmpl100.ipp
 * @brief XMPL100 driver implementation, included by `xmpl100.hpp`.
 * @copyright Copyright (c) 2026 The Kokeb Labs Authors. SPDX-License-Identifier: MIT
 *
 * Section numbers refer to the XMPL100 datasheet.
 */
#ifndef XMPL100_IPP
#define XMPL100_IPP

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
  return writeRegister(reg::kCtrl, detail::CtrlValue(shutdown, false));
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
  const auto raw = readStatusByte();
  if (!raw) {
    return Result<Status>::Failure(raw.error);
  }
  return Result<Status>::Success(detail::DecodeStatus(raw.value));
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
  const auto burst = std::span{bytes};
  const Error e = config_.crc_enable ? bus_.Read(reg::kTempMsb, burst)
                                     : bus_.Read(reg::kTempMsb, burst.first<2>());
  if (e != Error::Ok) {
    return Result<int16_t>::Failure(e);
  }
  if (config_.crc_enable && reg::Crc8(burst.first<2>()) != bytes[2]) {
    return Result<int16_t>::Failure(Error::Crc);
  }
  const auto counts = static_cast<int16_t>(static_cast<uint16_t>(
      (static_cast<uint32_t>(bytes[0]) << 8U) | static_cast<uint32_t>(bytes[1])));
  return Result<int16_t>::Success(counts);
}

template <SpiPort Port>
Result<float> XMPL100<Port>::ReadTemperatureC() noexcept {
  const auto raw = ReadRawTemperature();
  if (!raw) {
    return Result<float>::Failure(raw.error);
  }
  return Result<float>::Success(ToCelsius(raw.value, kTemperatureResolution));
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
  return writeRegister(reg::kCtrl, detail::CtrlValue(one_shot, true));
}

template <SpiPort Port>
Result<void> XMPL100<Port>::WaitDataReady() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  // One poll per millisecond, timeout + 1 polls. The bound is the validated timeout, capped by the
  // same maximum, so the loop ends even for a configuration that bypassed Validate()
  // (KB-CPP20-058).
  const uint32_t timeout_ms = config_.conversion_timeout_ms < kConversionTimeoutMaxMs
                                  ? config_.conversion_timeout_ms
                                  : kConversionTimeoutMaxMs;
  const uint32_t polls = timeout_ms + 1U;
  for (uint32_t poll = 0U; poll < polls; ++poll) {
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
Result<uint8_t> XMPL100<Port>::ReadRegister(uint8_t address) noexcept {
  if (!initialized_) {
    return Result<uint8_t>::Failure(Error::NotInitialized);
  }
  if (address > reg::kLastRegister) {
    return Result<uint8_t>::Failure(Error::InvalidArgument);
  }
  return address == reg::kStatus ? readStatusByte() : readRegister(address);
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
  // Clear STATUS.POR, so that a later POR means an unexpected reset; the latch starts empty, since
  // this POR is the reset just made (and anything latched before the start is not news either).
  if (auto st = readStatusByte(); !st) {
    return fail(st.error, "xmpl100: STATUS read failed");
  }
  latched_status_ = 0;
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
    const auto status = readStatusByte();
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
  if (auto r = writeRegister(reg::kCtrl, detail::CtrlValue(idle, false)); !r) {
    return r;
  }
  // Every caller passes a configuration that passed Validate(), which keeps each value inside its
  // register, so each conversion succeeds.
  const auto high_counts = ToCounts(config.alert_high_c, kTemperatureResolution);
  const auto low_counts = ToCounts(config.alert_low_c, kTemperatureResolution);
  const auto offset_counts = ToCounts(config.offset_c, kOffsetResolution);
  // Two's complement counts as 16-bit words, widened unsigned before the shifts (Rule 7.0.5).
  const uint32_t high = static_cast<uint16_t>(high_counts.value);
  const uint32_t low = static_cast<uint16_t>(low_counts.value);
  const std::array<uint8_t, 4> thresholds = {
      static_cast<uint8_t>(high >> 8U), static_cast<uint8_t>(high & 0xFFU),
      static_cast<uint8_t>(low >> 8U), static_cast<uint8_t>(low & 0xFFU)};
  if (const Error e = bus_.Write(reg::kAlertHighMsb, std::span{thresholds}); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  const auto offset = static_cast<uint8_t>(offset_counts.value);
  if (auto r = writeRegister(reg::kOffset, offset); !r) {
    return r;
  }
  if (auto r = writeRegister(reg::kConfig, detail::ConfigValue(config)); !r) {
    return r;
  }
  if (auto r = writeRegister(reg::kCtrl, detail::CtrlValue(config, false)); !r) {
    return r;
  }
  config_ = config;
  return Result<void>::Success();
}

template <SpiPort Port>
Result<uint8_t> XMPL100<Port>::readRegister(uint8_t reg) noexcept {
  std::array<uint8_t, 1> value{};
  if (const Error e = bus_.Read(reg, std::span{value}); e != Error::Ok) {
    return Result<uint8_t>::Failure(e);
  }
  return Result<uint8_t>::Success(value[0]);
}

template <SpiPort Port>
Result<uint8_t> XMPL100<Port>::readStatusByte() noexcept {
  // Every STATUS read goes through here: a read clears POR on the chip, so the driver keeps it.
  const auto status = readRegister(reg::kStatus);
  if (status) {
    last_status_ = status.value;
    latched_status_ =
        static_cast<uint8_t>(static_cast<uint32_t>(latched_status_) |
                             (static_cast<uint32_t>(status.value) & reg::kStatusClearOnRead));
  }
  return status;
}

template <SpiPort Port>
Result<void> XMPL100<Port>::writeRegister(uint8_t reg, uint8_t value) noexcept {
  const std::array<uint8_t, 1> data{value};
  if (const Error e = bus_.Write(reg, std::span{data}); e != Error::Ok) {
    return Result<void>::Failure(e);
  }
  return Result<void>::Success();
}

template <SpiPort Port>
Result<void> XMPL100<Port>::fail(Error error, const char* message) noexcept {
  detail::Log(bus_.GetPort(), LogLevel::Error, message);
  return Result<void>::Failure(error);
}

} // namespace xmpl100

#endif // XMPL100_IPP
