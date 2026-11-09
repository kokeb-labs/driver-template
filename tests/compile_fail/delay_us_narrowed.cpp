/**
 * @file delay_us_narrowed.cpp
 * @brief Must NOT compile: a DelayUs hook with a narrower parameter would truncate long waits, so
 * the driver rejects it instead of using it (handbook KB-CPP20-007).
 *
 * EXPECT: DelayUs hook must be
 */
#include <cstdint>

#include "mock_xmpl100.hpp"
#include "xmpl100.hpp"

class ShortDelayPort : public xmpl100_test::MockPortBase<ShortDelayPort> {
public:
  void DelayUs(uint16_t us) noexcept {
    (void)us;
  }
};

int main() {
  ShortDelayPort port;
  xmpl100::XMPL100<ShortDelayPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
