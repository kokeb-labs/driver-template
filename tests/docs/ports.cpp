/**
 * @file ports.cpp
 * @brief The port with every optional hook, as shown on the Porting page. Compiled, never run.
 */
#include <cstddef>
#include <cstdint>

#include "xmpl100.hpp"

bool platform_spi_init() noexcept;
bool platform_spi_transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept;
void platform_delay_us(uint32_t us) noexcept;
bool platform_gpio_write(int pin, bool high) noexcept;
void platform_log(const char* line) noexcept;

// --8<-- [start:full-port]
class FullPort : public xmpl100::SpiInterface<FullPort> {
public:
  bool EnsureInitialized() noexcept {
    return platform_spi_init();
  }
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return platform_spi_transfer(tx, rx, len);
  }

  // Required by every call that waits (reset, conversions).
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

  // Optional: RESET_N on a GPIO. ACTIVE means "in reset", which is a low level on the pin.
  bool GpioSet(xmpl100::CtrlPin pin, xmpl100::GpioSignal signal) noexcept {
    if (pin != xmpl100::CtrlPin::Reset) {
      return false;
    }
    return platform_gpio_write(kResetGpio, signal == xmpl100::GpioSignal::INACTIVE);
  }

  // Optional: one line per lifecycle event or failure. The driver never formats; the port does.
  void Log(xmpl100::LogLevel level, const char* message) noexcept {
    if (level > xmpl100::LogLevel::Warning) {
      return;
    }
    platform_log(message); // plain text: never use it as a format string
  }

private:
  static constexpr int kResetGpio = 7;
};
// --8<-- [end:full-port]

// --8<-- [start:hardware-reset]
xmpl100::Error RecoverWithResetPin(xmpl100::XMPL100<FullPort>& sensor) {
  // With GpioSet() this pulses RESET_N; a port without it gets Error::Unsupported.
  return sensor.HardwareReset().error;
}
// --8<-- [end:hardware-reset]
