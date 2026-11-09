/**
 * @file my_port.hpp
 * @brief The SPI port shown on the Quickstart and Porting pages, compiled by the host build.
 *
 * Every platform call goes through the `platform_*` functions declared below; a real port replaces
 * them with its SDK (ESP-IDF, STM32 HAL, Zephyr, Linux spidev).
 */
#ifndef XMPL100_MY_PORT_HPP
#define XMPL100_MY_PORT_HPP

#include <cstddef>
#include <cstdint>

#include "xmpl100_spi_interface.hpp"

// Your SDK's calls. Declared only: the docs build checks that the port compiles.
bool platform_spi_init() noexcept; // mode 0, <= 10 MHz
bool platform_spi_transfer(const uint8_t* tx, uint8_t* rx,
                           std::size_t len) noexcept; // CS low for all bytes
void platform_delay_us(uint32_t us) noexcept;         // busy-wait or sleep

// --8<-- [start:port]
class MyPort : public xmpl100::SpiInterface<MyPort> {
public:
  // Bring the bus up once; later calls return true at once.
  bool EnsureInitialized() noexcept {
    if (!ready_) {
      ready_ = platform_spi_init();
    }
    return ready_;
  }

  // One transfer, CS held low for all `len` bytes. `rx` is nullptr for writes.
  bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept {
    return platform_spi_transfer(tx, rx, len);
  }

  // Wait at least `us` microseconds: the chip's reset and conversion times depend on it.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

private:
  bool ready_ = false;
};
// --8<-- [end:port]

#endif // XMPL100_MY_PORT_HPP
