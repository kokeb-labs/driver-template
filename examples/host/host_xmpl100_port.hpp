/**
 * @file host_xmpl100_port.hpp
 * @brief The host port: the driver's bus hooks over a simulated XMPL100, so the example runs on any
 * PC.
 *
 * `xmpl100_test::MockPortBase` (tests/mock_xmpl100_bus.hpp) supplies the required bus hooks over
 * the register model the host tests use. This class adds what a real port adds: `DelayUs()` (here
 * it advances the simulated chip's clock instead of waiting) and `Log()` to the console. A port for
 * a real bus on a PC (Linux spidev, a USB bridge) implements the same hooks: see docs/porting.md.
 */
#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>

#include "mock_xmpl100.hpp"

/// The host port: simulated chip, real console.
class HostXmpl100Port : public xmpl100_test::MockPortBase<HostXmpl100Port> {
public:
  /// Time passes in the simulated chip; nothing sleeps.
  void DelayUs(uint32_t us) noexcept {
    state.chip.Elapse(us);
  }

  /// Driver messages (they carry their own `xmpl100:` prefix) go to the console, one line each.
  void Log(xmpl100::LogLevel level, const char* fmt, ...) noexcept {
    (void)level;
    std::va_list args;
    va_start(args, fmt);
    std::vprintf(fmt, args);
    std::fputs("\n", stdout);
    va_end(args);
  }
};
