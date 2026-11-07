/**
 * @file gpio_set_returns_void.cpp
 * @brief Must NOT compile: an optional hook with the wrong signature is rejected, not ignored.
 *
 * EXPECT: GpioSet hook must be
 */
#include "mock_xmpl100.hpp"
#include "xmpl100.hpp"

class VoidGpioPort : public xmpl100_test::MockPortBase<VoidGpioPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
  void GpioSet(xmpl100::CtrlPin pin, xmpl100::GpioSignal signal) noexcept {
    (void)pin;
    (void)signal;
  }
};

int main() {
  VoidGpioPort port;
  xmpl100::XMPL100<VoidGpioPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
