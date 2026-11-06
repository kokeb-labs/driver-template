# XMPL100 Driver

C++20 driver for the XMPL100, a fictional example SPI temperature sensor: chip ID, one-shot and continuous reads, CRC.

[![CI](https://github.com/kokeb-labs/driver-template/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kokeb-labs/driver-template/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

**Documentation: <https://kokeb-labs.github.io/driver-template/>**

<!-- scaffold:remove -->
> This is the template for drivers and libraries. Create a repository from it and run `python3 tools/setup.py`:
> see [Using the template](docs/template.md).
<!-- /scaffold:remove -->

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/driver-layers.dark.svg">
  <img src="docs/diagrams/driver-layers.svg"
    alt="Layer diagram: your application calls the XMPL100 core, which sends register reads and writes
    through the SPI port contract to the port class you write, which drives the chip; the core and the
    contract are the same on every MCU.">
</picture>
<!-- /figure -->

## Try it on your PC

No hardware needed: [`examples/host`](examples/host/README.md) runs the driver against a simulated chip.

```bash
cmake -S examples/host -B build-example && cmake --build build-example && ./build-example/xmpl100_host_example
```

## In 30 seconds

Write a small port class for your MCU (bus bring-up and byte transfers;
[Porting](https://kokeb-labs.github.io/driver-template/latest/porting/)), then:

<!-- generated: quickstart-code -->
```cpp
#include "xmpl100.hpp"

int read_board_temperature(MyPort& port, float& celsius) {
  xmpl100::XMPL100<MyPort> sensor(port); // no I/O yet

  if (auto r = sensor.Initialize(); !r) { // 1. bus up, reset, CHIP_ID, Config{}
    return static_cast<int>(r.error);     //    Bus, Timeout or DeviceIdMismatch: check wiring
  }
  const auto t = sensor.ReadTemperatureC(); // 2. latest continuous result
  if (!t) {
    return static_cast<int>(t.error); //    DeviceFault: sensing element open or shorted
  }
  celsius = t.value; // 3. degrees Celsius, 1/128 °C resolution
  return 0;
}
```
<!-- /generated -->

## What it does

- Brings the chip up with a soft reset and a CHIP_ID check; nothing touches the chip before that succeeds.
- Reads the temperature in continuous mode, or one conversion on demand, with a bounded wait.
- Checks the CRC-8 of every result on revision B silicon and reports sensor faults.
- Programs the alert thresholds and the offset from a `Config` with datasheet defaults and three presets.
- Returns `Result<T>` with an `Error` from every call: no exceptions, no heap, no RTTI, no virtual calls.

## Install

```bash
git submodule add https://github.com/kokeb-labs/driver-template.git external/driver-template
```

```cmake
add_subdirectory(external/driver-template)
target_link_libraries(my_app PRIVATE xmpl100::xmpl100)
```

Plain CMake covers any MCU (STM32, RP2040, nRF, ...) and the host.
<!-- scaffold:platform-esp32 -->
Under ESP-IDF the repository root is itself a component: add it to `EXTRA_COMPONENT_DIRS`, check it out under
`components/`, or use it as a component-manager git dependency.
<!-- /scaffold:platform-esp32 -->
Details: [Installation](https://kokeb-labs.github.io/driver-template/latest/installation/) and
[Platforms](https://kokeb-labs.github.io/driver-template/latest/platforms/).

## Documentation

| | |
| --- | --- |
| [Quickstart](https://kokeb-labs.github.io/driver-template/latest/quickstart/) | Wire it, write the port, read a temperature |
| [Hardware](https://kokeb-labs.github.io/driver-template/latest/hardware/) | Pins, bus settings, timing, power |
| [Contract](https://kokeb-labs.github.io/driver-template/latest/contract/) | Promises, dependencies, threading, errors |
| [API guide](https://kokeb-labs.github.io/driver-template/latest/api-guide/) | Initialise, configure, measure, handle errors |
| [Porting](https://kokeb-labs.github.io/driver-template/latest/porting/) | The port and its optional hooks |
| [API reference](https://kokeb-labs.github.io/driver-template/latest/api/) | Doxygen, every class and member |

## Tests

```bash
cmake -S . -B build -DXMPL100_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
```

## License

MIT. See [LICENSE](LICENSE).
