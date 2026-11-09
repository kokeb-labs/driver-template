/**
 * @file fuzz_xmpl100.cpp
 * @brief Fuzz target: the driver over a bus whose behaviour the fuzzer controls.
 *
 * What the driver reads comes from the chip over the bus, so read data is the untrusted input. The
 * fuzzer's bytes choose a configuration, then a sequence of driver calls; on every transfer they
 * also choose what the bus does: pass it to the chip model, fail it, flip bits of the data read or
 * replace it. They can also change the chip: a wrong CHIP_ID, revision A silicon, a sensor fault, a
 * stuck reset or conversion, a corrupted CRC, any temperature, a brown-out.
 *
 * After every call the target checks the driver's promises (docs/contract.md), and aborts, naming
 * the line, when one breaks:
 *
 * - a failed call returns an `Error` the enum defines, and `T{}`;
 * - before `EnsureInitialized()` succeeds, no call makes a transfer: reads return
 *   `NotInitialized`, setters only store the configuration;
 * - `InvalidArgument` means nothing was sent;
 * - a temperature is inside the range of the 16-bit result, -256 °C to +256 °C;
 * - a started driver with CRC enabled runs on revision B silicon or later;
 * - `UnexpectedReset` leaves the driver uninitialised;
 * - every log line is a non-empty plain-text message.
 *
 * Built two ways (tests/fuzz/CMakeLists.txt): with libFuzzer (Clang, `-DXMPL100_BUILD_FUZZERS=ON`),
 * and as a plain executable that replays the corpus in tests/fuzz/corpus/ as a CTest on any
 * compiler.
 */
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include "mock_xmpl100.hpp" // the chip model, then the bus mock in mock_xmpl100_bus.hpp
#include "xmpl100.hpp"

namespace {

using xmpl100::Error;

/// The fuzzer's bytes, read front to back; zeros once they run out.
class Input {
public:
  Input(const uint8_t* data, std::size_t size) noexcept : data_(data), size_(size) {}

  [[nodiscard]] bool Empty() const noexcept {
    return pos_ >= size_;
  }
  uint8_t Byte() noexcept {
    return pos_ < size_ ? data_[pos_++] : uint8_t{0};
  }
  uint16_t Word() noexcept {
    const auto high = Byte();
    return static_cast<uint16_t>((static_cast<uint32_t>(high) << 8U) | Byte());
  }

private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

/// A broken promise: name the line and abort, so libFuzzer saves the input as a crash.
void Check(bool promise, int line = __builtin_LINE()) noexcept {
  if (!promise) {
    (void)std::fprintf(stderr, "fuzz_xmpl100: promise broken at line %d\n", line);
    std::abort();
  }
}

/// What the bus does with one transfer, chosen by the fuzzer; any other value passes it.
constexpr uint8_t kFail = 0xFF;    ///< The transfer fails; it never reached the chip.
constexpr uint8_t kFlip = 0xFE;    ///< Data read: bits flipped (noise). A write: lost, where the
                                   ///< bus acknowledges writes.
constexpr uint8_t kReplace = 0xFD; ///< The data read is replaced (another device answered).

/// The port: the chip model of the host tests behind a bus the fuzzer controls.
class FuzzPort : public xmpl100::SpiInterface<FuzzPort> {
public:
  explicit FuzzPort(Input& in) noexcept : in_(in) {}

  [[nodiscard]] bool EnsureInitialized() const noexcept {
    return bus_up_ok_;
  }

  // scaffold:fuzz-port
  /// One SPI frame. A write has no `rx`; the reply of a read is what the fuzzer lets through.
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    Check(tx != nullptr && len >= 2 && len <= 1 + xmpl100::detail::kMaxBurst);
    const uint8_t action = nextAction();
    if (action == kFail) {
      return false;
    }
    (void)bus_.DoTransfer(tx, rx, len);
    if (rx != nullptr) {
      tamper(action, rx, len);
    }
    return true;
  }
  // /scaffold:fuzz-port

  void DelayUs(uint32_t us) noexcept {
    bus_.chip.Elapse(us);
  }

  /// RESET_N of the model; the fuzzer may fail the pin write.
  bool GpioSet(xmpl100::CtrlPin pin, xmpl100::GpioSignal signal) noexcept {
    Check(pin == xmpl100::CtrlPin::Reset);
    if (nextAction() == kFail) {
      return false;
    }
    const bool assert_reset = signal == xmpl100::GpioSignal::ACTIVE;
    if (reset_asserted_ && !assert_reset) {
      bus_.chip.Reset(); // released: the chip runs its reset sequence
    }
    reset_asserted_ = assert_reset;
    return true;
  }

  /// The driver logs plain text: one non-empty line, never a format string.
  void Log(xmpl100::LogLevel level, const char* message) noexcept {
    (void)level;
    Check(message != nullptr && message[0] != '\0');
  }

  xmpl100_test::Xmpl100Model& Chip() noexcept {
    return bus_.chip;
  }
  /// Transfers and pin writes so far.
  [[nodiscard]] int Transfers() const noexcept {
    return transfers_;
  }
  void SetBusUp(bool ok) noexcept {
    bus_up_ok_ = ok;
  }

private:
  /// Count one bus access and let the fuzzer decide what happens to it.
  uint8_t nextAction() noexcept {
    transfers_++;
    return in_.Byte();
  }

  /// Apply the fuzzer's choice to `len` bytes that came from the chip.
  void tamper(uint8_t action, uint8_t* data, std::size_t len) noexcept {
    for (std::size_t i = 0; i < len; ++i) {
      if (action == kFlip) {
        data[i] = static_cast<uint8_t>(data[i] ^ in_.Byte());
      } else if (action == kReplace) {
        data[i] = in_.Byte();
      } else {
        // Pass the byte through unchanged.
      }
    }
  }

  Input& in_;
  xmpl100_test::MockBusState bus_;
  int transfers_ = 0;
  bool bus_up_ok_ = true;
  bool reset_asserted_ = false;
};

using Driver = xmpl100::XMPL100<FuzzPort>;

bool KnownError(Error e) noexcept {
  switch (e) {
  case Error::Ok:
  case Error::NotInitialized:
  case Error::InvalidArgument:
  case Error::Bus:
  case Error::Timeout:
  case Error::DeviceIdMismatch:
  case Error::Crc:
  case Error::Busy:
  case Error::DeviceFault:
  case Error::Unsupported:
  case Error::UnexpectedReset:
    return true;
  default:
    break;
  }
  return false;
}

/// A threshold in °C, inside and outside the accepted range; 0xFF gives NaN.
float FuzzThreshold(Input& in) noexcept {
  const uint8_t b = in.Byte();
  if (b == 0xFFU) {
    return std::numeric_limits<float>::quiet_NaN();
  }
  return static_cast<float>(static_cast<int8_t>(b)) * 2.0F;
}

/// A configuration from the fuzzer's bytes; sometimes invalid on purpose.
xmpl100::Config FuzzConfig(Input& in) noexcept {
  xmpl100::Config c{};
  const uint8_t flags = in.Byte();
  c.mode = static_cast<xmpl100::Mode>(flags % 4U); // 3 is not an enumerator
  c.crc_enable = (flags & 0x04U) != 0U;
  c.alert_enable = (flags & 0x08U) != 0U;
  c.alert_polarity = static_cast<xmpl100::AlertPolarity>((flags >> 4U) % 3U);
  if ((flags & 0x80U) != 0U) {
    c.odr = static_cast<xmpl100::OutputDataRate>(in.Byte() % 6U);
    c.averaging = static_cast<xmpl100::Averaging>(in.Byte() % 5U);
    c.alert_low_c = FuzzThreshold(in);
    c.alert_high_c = FuzzThreshold(in);
    // -8 to +7.9375 °C in OFFSET steps; past the end of that range when 0x40 is set.
    c.offset_c = static_cast<float>(static_cast<int8_t>(in.Byte())) / 16.0F;
    if ((flags & 0x40U) != 0U) {
      c.offset_c *= 2.0F;
    }
    c.conversion_timeout_ms = in.Byte() % 20U; // 0 is invalid; short, so a stuck chip ends fast
  }
  return c;
}

/// Change the chip between two calls.
void FuzzChip(xmpl100_test::Xmpl100Model& chip, Input& in) noexcept {
  switch (in.Byte() % 8U) {
  case 0:
    chip.chip_id = (in.Byte() & 1U) != 0U ? in.Byte() : xmpl100::kChipId;
    break;
  case 1:
    chip.rev_id = (in.Byte() & 1U) != 0U ? xmpl100::kRevisionA : xmpl100::kRevisionB;
    break;
  case 2:
    chip.temperature_counts = static_cast<int16_t>(in.Word());
    break;
  case 3:
    chip.fault = !chip.fault;
    break;
  case 4:
    chip.corrupt_crc = !chip.corrupt_crc;
    break;
  case 5:
    chip.reset_stuck = !chip.reset_stuck;
    break;
  case 6:
    chip.conversion_stuck = !chip.conversion_stuck;
    break;
  default:
    chip.PowerOn(); // a brown-out: reset values, STATUS.POR set
    break;
  }
}

template <typename T>
bool IsEmpty(const T& value) noexcept {
  return value == T{};
}

bool IsEmpty(const xmpl100::Status& s) noexcept {
  return !s.data_ready && !s.busy && !s.alert_high && !s.alert_low && !s.power_on_reset && !s.fault;
}

/// The promises of a call that needs a started driver.
template <typename R>
void CheckCommon(const Driver& d, const R& r, bool was_up, int sent) noexcept {
  Check(KnownError(r.error));
  if (!was_up) {
    Check(r.error == Error::NotInitialized && sent == 0 && !d.IsInitialized());
  }
  if (r.error == Error::InvalidArgument) {
    Check(sent == 0);
  }
  if (r.error == Error::UnexpectedReset) {
    Check(!d.IsInitialized());
  }
}

/// A failed read returns `T{}`.
template <typename T>
void CheckValue(const xmpl100::Result<T>& r) noexcept {
  if (!r) {
    Check(IsEmpty(r.value));
  }
}

/// A temperature is a 16-bit count of 1/128 °C.
void CheckTemperature(const xmpl100::Result<float>& r) noexcept {
  CheckValue(r);
  if (r) {
    Check(r.value >= -256.0F && r.value < 256.0F);
  }
}

/// A setter: only stored before the start; nothing sent when refused.
void CheckSetter(const xmpl100::Result<void>& r, bool was_up, int sent) noexcept {
  Check(KnownError(r.error));
  if (!was_up) {
    Check((r.ok() || r.error == Error::InvalidArgument) && sent == 0);
  }
  if (r.error == Error::InvalidArgument || r.error == Error::Unsupported) {
    Check(sent == 0);
  }
}

/// A start or reset: initialised exactly when it succeeded, CRC only on revision B.
void CheckStarted(const Driver& d, const xmpl100::Result<void>& r) noexcept {
  Check(KnownError(r.error) && r.ok() == d.IsInitialized());
  if (r.ok() && d.GetConfig().crc_enable) {
    Check(d.GetRevision() >= xmpl100::kRevisionB);
  }
}

void RunOne(Driver& d, FuzzPort& port, Input& in) noexcept {
  const uint8_t op = in.Byte() % 14U;
  const bool was_up = d.IsInitialized();
  const int before = port.Transfers();
  const auto sent = [&] { return port.Transfers() - before; };

  switch (op) {
  case 0: {
    const auto r = d.EnsureInitialized();
    CheckStarted(d, r);
    if (was_up || r.error == Error::InvalidArgument) {
      Check(r.ok() == was_up && sent() == 0);
    }
    break;
  }
  case 1: {
    const auto r = d.Deinitialize();
    Check(KnownError(r.error) && !d.IsInitialized());
    if (!was_up) {
      Check(r.ok() && sent() == 0);
    }
    break;
  }
  case 2: {
    const auto r = d.SoftReset();
    CheckCommon(d, r, was_up, sent());
    CheckStarted(d, r);
    break;
  }
  case 3: {
    const auto r = d.HardwareReset();
    CheckCommon(d, r, was_up, sent());
    CheckStarted(d, r);
    break;
  }
  case 4:
    CheckSetter(d.ApplyConfig(FuzzConfig(in)), was_up, sent());
    break;
  case 5:
    CheckSetter(d.SetMode(static_cast<xmpl100::Mode>(in.Byte() % 4U)), was_up, sent());
    break;
  case 6: {
    const float low = FuzzThreshold(in);
    CheckSetter(d.SetAlertThresholdsC(low, FuzzThreshold(in)), was_up, sent());
    break;
  }
  case 7: {
    const auto r = d.ReadStatus();
    CheckCommon(d, r, was_up, sent());
    CheckValue(r);
    break;
  }
  case 8: {
    const auto r = d.ReadRawTemperature();
    CheckCommon(d, r, was_up, sent());
    CheckValue(r);
    break;
  }
  case 9: {
    const auto r = d.ReadTemperatureC();
    CheckCommon(d, r, was_up, sent());
    CheckTemperature(r);
    break;
  }
  case 10:
    CheckCommon(d, d.TriggerOneShot(), was_up, sent());
    break;
  case 11:
    CheckCommon(d, d.WaitDataReady(), was_up, sent());
    break;
  case 12: {
    const auto r = d.MeasureOnceC();
    CheckCommon(d, r, was_up, sent());
    CheckTemperature(r);
    break;
  }
  default:
    FuzzChip(port.Chip(), in);
    port.SetBusUp((in.Byte() & 0x07U) != 0U);
    break;
  }
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  Input in(data, size);
  FuzzPort port(in);
  Driver driver(port, FuzzConfig(in));
  for (int calls = 0; calls < 64 && !in.Empty(); ++calls) {
    RunOne(driver, port, in);
  }
  return 0;
}
