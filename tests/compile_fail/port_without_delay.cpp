/**
 * @file port_without_delay.cpp
 * @brief Must NOT compile: the chip's reset and conversion waits need the port's DelayUs().
 *
 * EXPECT: DelayUs
 */
#include "mock_xmpl100.hpp"
#include "xmpl100.hpp"

class NoDelayPort : public xmpl100_test::MockPortBase<NoDelayPort> {};

int main() {
  NoDelayPort port;
  xmpl100::XMPL100<NoDelayPort> sensor(port);
  return static_cast<int>(sensor.Initialize().error);
}
