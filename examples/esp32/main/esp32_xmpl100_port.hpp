/**
 * @file esp32_xmpl100_port.hpp
 * @brief ESP-IDF port for the XMPL100 driver: spi_master for the bus, GPIO for RESET_N, esp_rom
 * delays and ESP_LOG for the log hook.
 *
 * Start from this file for any ESP-IDF target. The pins are in Esp32Xmpl100Pins; the defaults match
 * the wiring table in examples/esp32/README.md (ESP32-C6 DevKitC-1).
 */
#ifndef XMPL100_ESP32_XMPL100_PORT_HPP
#define XMPL100_ESP32_XMPL100_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "xmpl100.hpp"

/// The port's default SPI clock: 1 MHz for bring-up on jumper wires.
inline constexpr int kEsp32Xmpl100ClockHz = 1'000'000;
/// The port's SPI mode.
inline constexpr int kEsp32Xmpl100SpiMode = 0;
// The bus requirements of the driver (handbook KB-CPP-123), checked at compile time.
static_assert(kEsp32Xmpl100ClockHz <= static_cast<int>(xmpl100::kMaxClockHz),
              "the default clock is above the XMPL100's f_SCK max");
static_assert(kEsp32Xmpl100SpiMode == xmpl100::kSpiMode || kEsp32Xmpl100SpiMode == 3,
              "the XMPL100 samples in SPI mode 0 or 3");
static_assert(xmpl100::kMsbFirst, "this port sends MSB first (no SPI_DEVICE_BIT_LSBFIRST)");

/// Pins and bus settings of the ESP-IDF port.
struct Esp32Xmpl100Pins {
  spi_host_device_t host = SPI2_HOST;
  int sclk = 6;
  int mosi = 7;
  int miso = 2;
  int cs = 10;
  int reset_n = 11;                    ///< -1 when RESET_N is tied high.
  int clock_hz = kEsp32Xmpl100ClockHz; ///< Up to `xmpl100::kMaxClockHz`; checked at start.
  int spi_mode = kEsp32Xmpl100SpiMode; ///< Mode 0 or 3.
};

/// The XMPL100 SPI port on ESP-IDF, with every optional hook.
class Esp32Xmpl100Port : public xmpl100::SpiInterface<Esp32Xmpl100Port> {
public:
  explicit Esp32Xmpl100Port(const Esp32Xmpl100Pins& pins = {}) noexcept : pins_(pins) {}

  bool EnsureInitialized() noexcept {
    if (device_ != nullptr) {
      return true;
    }
    if (pins_.clock_hz <= 0 || pins_.clock_hz > static_cast<int>(xmpl100::kMaxClockHz) ||
        (pins_.spi_mode != 0 && pins_.spi_mode != 3)) {
      ESP_LOGE(kTag, "clock or SPI mode outside the XMPL100's bus requirements");
      return false;
    }
    spi_bus_config_t bus{};
    bus.mosi_io_num = pins_.mosi;
    bus.miso_io_num = pins_.miso;
    bus.sclk_io_num = pins_.sclk;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 16;
    const esp_err_t bus_err = spi_bus_initialize(pins_.host, &bus, SPI_DMA_DISABLED);
    if (bus_err != ESP_OK && bus_err != ESP_ERR_INVALID_STATE) { // INVALID_STATE: bus already up
      ESP_LOGE(kTag, "spi_bus_initialize: %s", esp_err_to_name(bus_err));
      return false;
    }
    spi_device_interface_config_t dev{};
    dev.clock_speed_hz = pins_.clock_hz;
    dev.mode = static_cast<uint8_t>(pins_.spi_mode);
    dev.spics_io_num = pins_.cs;
    dev.queue_size = 1;
    if (spi_bus_add_device(pins_.host, &dev, &device_) != ESP_OK) {
      device_ = nullptr;
      return false;
    }
    if (pins_.reset_n >= 0) {
      gpio_config_t io{};
      io.pin_bit_mask = 1ULL << static_cast<unsigned>(pins_.reset_n);
      io.mode = GPIO_MODE_OUTPUT;
      // RESET_N is driven to its inactive level (high) before its output is enabled, so configuring
      // the pin never pulses a reset (handbook KB-HW-018).
      if (gpio_set_level(static_cast<gpio_num_t>(pins_.reset_n), 1) != ESP_OK ||
          gpio_config(&io) != ESP_OK) {
        return false;
      }
    }
    return true;
  }

  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    if (device_ == nullptr || len == 0) {
      return false;
    }
    spi_transaction_t t{};
    t.length = len * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx; // nullptr for writes: MISO is discarded
    return spi_device_polling_transmit(device_, &t) == ESP_OK;
  }

  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }

  bool GpioSet(xmpl100::CtrlPin pin, xmpl100::GpioSignal signal) noexcept {
    if (pin != xmpl100::CtrlPin::Reset || pins_.reset_n < 0) {
      return false;
    }
    // RESET_N is active low: ACTIVE (in reset) is level 0.
    const uint32_t level = signal == xmpl100::GpioSignal::ACTIVE ? 0 : 1;
    return gpio_set_level(static_cast<gpio_num_t>(pins_.reset_n), level) == ESP_OK;
  }

  void Log(xmpl100::LogLevel level, const char* message) noexcept {
    const esp_log_level_t esp_level = level == xmpl100::LogLevel::Error     ? ESP_LOG_ERROR
                                      : level == xmpl100::LogLevel::Warning ? ESP_LOG_WARN
                                      : level == xmpl100::LogLevel::Info    ? ESP_LOG_INFO
                                                                            : ESP_LOG_DEBUG;
    esp_log_write(esp_level, kTag, "%s\n", message); // plain text, never a format
  }

private:
  static constexpr const char* kTag = "xmpl100";
  Esp32Xmpl100Pins pins_;
  spi_device_handle_t device_ = nullptr;
};

#endif // XMPL100_ESP32_XMPL100_PORT_HPP
