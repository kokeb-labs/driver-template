/**
 * @file checked_config_invalid.cpp
 * @brief Must NOT compile: a CheckedConfig built from a configuration that Validate() rejects fails
 * the build, naming the error (handbook KB-CPP20-013).
 *
 * EXPECT: ConfigIsInvalid
 */
#include "xmpl100_types.hpp"

int main() {
  // 16 Hz cannot average 8 samples in continuous mode: Validate() rejects it.
  constexpr xmpl100::CheckedConfig kBoard{
      xmpl100::Config{.odr = xmpl100::OutputDataRate::Hz16, .averaging = xmpl100::Averaging::X8}};
  return static_cast<int>(kBoard.Get().odr);
}
