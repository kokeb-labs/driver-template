/**
 * @file not_a_port.cpp
 * @brief Must NOT compile: the driver is constrained by the port concept.
 *
 * EXPECT: Port
 */
#include "xmpl100.hpp"

struct NotAPort {
  bool EnsureInitialized() noexcept {
    return true;
  }
};

int main() {
  NotAPort port;
  xmpl100::XMPL100<NotAPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
