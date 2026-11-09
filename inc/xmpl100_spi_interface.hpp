// clang-format off: the table below must keep one row per line.
/**
 * @file xmpl100_spi_interface.hpp
 * @brief The SPI port of the XMPL100 driver: the CRTP base, its pin enums, the `SpiPort` concept
 * and the register framing.
 * @copyright Copyright (c) 2026 The Kokeb Labs Authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The driver never calls an SDK. It calls hooks on a class you derive from
 * `xmpl100::SpiInterface<Self>`:
 *
 * | Hook | Contract |
 * | --- | --- |
 * | `bool EnsureInitialized() noexcept` | Bring the bus up on the first call; `true` at once afterwards |
 * | `bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept` | One full-duplex transfer, CS low for all `len` bytes; `rx` may be `nullptr` |
 *
 * The port moves raw bytes: it never retries, never interprets the frame and never configures the
 * chip. What the chip needs from the bus is below as `constexpr` values (`kSpiMode` to
 * `kContinuousWord`, datasheet section 6.2); a port checks its settings against them with
 * `static_assert`. The optional hooks (`DelayUs`, `GpioSet`, `Log`) are described in `xmpl100.hpp`.
 */
// clang-format on
#ifndef XMPL100_SPI_INTERFACE_HPP
#define XMPL100_SPI_INTERFACE_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>

#include "xmpl100_types.hpp"

namespace xmpl100 {

/**
 * @name Bus requirements
 * What the XMPL100 needs from the SPI bus (datasheet section 6.2, Table 6-1). A port and a board
 * configuration check their settings against these with `static_assert` (handbook KB-CPP-123).
 * @{
 */
/// SPI mode the driver is tested with: CPOL 0, CPHA 0. The chip also accepts mode 3.
inline constexpr uint8_t kSpiMode = 0;
/// Each byte is clocked most significant bit first.
inline constexpr bool kMsbFirst = true;
/// Highest SCK frequency, f_SCK max.
inline constexpr uint32_t kMaxClockHz = 10'000'000;
/// Shortest time CS stays high between two transfers, t_CSH.
inline constexpr uint32_t kMinCsHighNs = 50;
/// CS low to the first SCK edge, t_CSS.
inline constexpr uint32_t kCsSetupNs = 20;
/// Last SCK edge to CS high, t_CSHD.
inline constexpr uint32_t kCsHoldNs = 20;
/// `false`: the chip accepts idle SCK time between the bytes of one transfer.
inline constexpr bool kContinuousWord = false;
/** @} */

/**
 * @brief Control pins the port may drive through the optional `GpioSet()` hook.
 *
 * The port drives each pin to INACTIVE before it enables the pin's output and before the first
 * transfer (handbook KB-HW-018), so the chip never sees a reset pulse it did not ask for.
 */
enum class CtrlPin : uint8_t {
  Reset = 0, ///< RESET_N, active low on the package pin; idles high (INACTIVE). ACTIVE holds the
             ///< chip in reset.
};

/** @brief Logical level of a control pin; the port maps it to the physical polarity. */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< The pin function is deasserted.
  ACTIVE = 1,   ///< The pin function is asserted.
};

/**
 * @brief CRTP base of the platform SPI port.
 *
 * Derive `class MyPort : public xmpl100::SpiInterface<MyPort>` and implement the hooks in this
 * file's description. The base declares no hook itself (a forwarding hook would turn a missing
 * override into infinite recursion) and no data; the driver checks your class with the `SpiPort`
 * concept, so a missing or misdeclared hook is a compile error that names it.
 *
 * @tparam Derived Your port class.
 */
template <typename Derived>
class SpiInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  SpiInterface(const SpiInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  SpiInterface& operator=(const SpiInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  SpiInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~SpiInterface() noexcept = default;
};

/**
 * @brief Satisfied by a class that derives from `SpiInterface<Port>` and implements the required
 * hooks with the right types.
 * @tparam Port The port class.
 */
template <typename Port>
concept SpiPort = std::derived_from<Port, SpiInterface<Port>> &&
                  requires(Port& port, const uint8_t* tx, uint8_t* rx, std::size_t len) {
                    { port.EnsureInitialized() } -> std::same_as<bool>;
                    { port.Transfer(tx, rx, len) } -> std::same_as<bool>;
                  };

namespace detail {

/// First-byte flag of a read frame (datasheet 6.2.1: bit 7 = 1 read, 0 write; bits 6..0 address).
inline constexpr uint8_t kReadFlag = 0x80;
/// Address bits of the first byte.
inline constexpr uint8_t kAddressMask = 0x7F;
/// Longest register burst the driver issues, in data bytes.
inline constexpr std::size_t kMaxBurst = 4;

/// Register access over SPI: the framing of datasheet section 6.2, one transfer per access.
template <SpiPort Port>
class RegisterBus {
public:
  explicit RegisterBus(Port& port) noexcept : port_(port) {}

  Port& GetPort() noexcept {
    return port_;
  }

  /// Read `len` consecutive registers from `reg`. `Error::Bus` on a failed transfer.
  Error Read(uint8_t reg, uint8_t* out, std::size_t len) noexcept {
    if (len == 0 || len > kMaxBurst) {
      return Error::InvalidArgument;
    }
    std::array<uint8_t, 1 + kMaxBurst> tx{};
    std::array<uint8_t, 1 + kMaxBurst> rx{};
    tx[0] = static_cast<uint8_t>(static_cast<uint32_t>(kReadFlag) |
                                 (static_cast<uint32_t>(reg) & kAddressMask));
    if (!port_.Transfer(tx.data(), rx.data(), len + 1)) {
      return Error::Bus;
    }
    for (std::size_t i = 0; i < len; ++i) {
      out[i] = rx[1 + i];
    }
    return Error::Ok;
  }

  /// Write `len` consecutive registers from `reg`. `Error::Bus` on a failed transfer.
  Error Write(uint8_t reg, const uint8_t* data, std::size_t len) noexcept {
    if (len == 0 || len > kMaxBurst) {
      return Error::InvalidArgument;
    }
    std::array<uint8_t, 1 + kMaxBurst> tx{};
    tx[0] = static_cast<uint8_t>(static_cast<uint32_t>(reg) & kAddressMask);
    for (std::size_t i = 0; i < len; ++i) {
      tx[1 + i] = data[i];
    }
    return port_.Transfer(tx.data(), nullptr, len + 1) ? Error::Ok : Error::Bus;
  }

private:
  Port& port_;
};

} // namespace detail

} // namespace xmpl100

#endif // XMPL100_SPI_INTERFACE_HPP
