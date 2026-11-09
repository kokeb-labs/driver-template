/**
 * @file xmpl100_detail.hpp
 * @brief Compile-time helpers of the XMPL100 driver: exact hook probes, typed register fields and
 * enumeration checks.
 * @copyright Copyright (c) 2026 The driver-template authors. SPDX-License-Identifier: MIT
 *
 * @details
 * Everything here is `constexpr` or used only in unevaluated operands, so it adds no code to the
 * run-time path (handbook C++20 profile, KB-CPP20-006, -008, -020, -021, -025, -026).
 */
#ifndef XMPL100_DETAIL_HPP
#define XMPL100_DETAIL_HPP

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace xmpl100::detail {

/**
 * @brief Converts to exactly `T` and to nothing else, so a port concept that probes a hook with it
 * rejects a hook whose parameter is a narrower or different type (KB-CPP20-006).
 *
 * Declared only: it appears in requires-expressions, never in evaluated code.
 */
template <typename T>
struct Exactly {
  template <typename U>
    requires std::same_as<U, T>
  operator U() const noexcept; // NOLINT(google-explicit-constructor): the probe is the conversion
};

/// `false` for every `T`: the condition of a `static_assert` in a branch that must not be
/// instantiated (KB-CPP20-008).
template <typename>
inline constexpr bool kAlwaysFalse = false;

/**
 * @brief The value of an enumerator in its underlying type, our stand-in for C++23
 * `std::to_underlying` (KB-CPP20-002, -026).
 */
template <typename E>
  requires std::is_enum_v<E>
[[nodiscard]] constexpr std::underlying_type_t<E> ToUnderlying(E e) noexcept {
  return static_cast<std::underlying_type_t<E>>(e);
}

/**
 * @brief The last enumerator of a contiguous enumeration that starts at 0; specialised next to each
 * enumeration that `IsEnumerator()` checks (KB-CPP20-026).
 */
template <typename E>
struct EnumLast;

/**
 * @brief Whether `e` holds an enumerator of a contiguous enumeration that starts at 0. A value made
 * by `static_cast` from an integer can be anything its underlying type holds (KB-CPP20-025).
 */
template <typename E>
  requires std::is_enum_v<E>
[[nodiscard]] constexpr bool IsEnumerator(E e) noexcept {
  return static_cast<uint32_t>(ToUnderlying(e)) <=
         static_cast<uint32_t>(ToUnderlying(EnumLast<E>::kValue));
}

/**
 * @brief A bit field of a register: `Width` bits from bit `Shift` of a `Reg`. Every shift is a
 * constant below the width and every operand is widened to `uint32_t` once, by construction
 * (KB-CPP20-020, -021).
 */
template <typename Reg, uint32_t Shift, uint32_t Width>
  requires std::unsigned_integral<Reg> && (Width > 0U) && (Width < 32U) &&
           (Shift + Width <= static_cast<uint32_t>(std::numeric_limits<Reg>::digits))
struct Field {
  /// The largest value the field holds.
  static constexpr uint32_t kMax = (uint32_t{1} << Width) - 1U;
  /// The field's bits in the register.
  static constexpr Reg kMask = static_cast<Reg>(kMax << Shift);

  /// `reg` with the field set to `value` (masked to the field's width); the other bits unchanged.
  [[nodiscard]] static constexpr Reg Insert(Reg reg, uint32_t value) noexcept {
    return static_cast<Reg>((static_cast<uint32_t>(reg) & ~static_cast<uint32_t>(kMask)) |
                            ((value & kMax) << Shift));
  }
  /// The field's value in `reg`.
  [[nodiscard]] static constexpr uint32_t Extract(Reg reg) noexcept {
    return (static_cast<uint32_t>(reg) >> Shift) & kMax;
  }
};

} // namespace xmpl100::detail

#endif // XMPL100_DETAIL_HPP
