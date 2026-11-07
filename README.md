# XMPL100 Driver

C++20 driver for the XMPL100, a fictional example SPI temperature sensor: chip ID, one-shot and continuous reads, CRC.

[![CI](https://github.com/kokeb-labs/driver-template/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kokeb-labs/driver-template/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-informational)](docs/installation.md#requirements)

**Documentation: <https://kokeb-labs.github.io/driver-template/>**

<!-- scaffold:remove -->
> This is the template for drivers and libraries. Create a repository from it and run `python3 tools/setup.py`:
> see [Using the template](docs/template.md).
<!-- /scaffold:remove -->

The XMPL100 measures the temperature of its own die, from -40 °C to +125 °C in steps of 1/128 °C, and raises an
alert pin when it leaves a window you set. This driver starts it safely, reads it continuously or on demand, checks
the CRC of every result and turns the chip's status flags into errors you can act on. It runs on any MCU: you give
it a small class that moves bytes on your bus, and it does the rest.

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/diagrams/driver-layers.dark.svg">
  <img src="docs/diagrams/driver-layers.svg"
    alt="Layer diagram: your application calls the XMPL100 core, which sends register reads and writes
    through the SPI port contract to the port class you write, which drives the chip; the core and the
    contract are the same on every MCU.">
</picture>
<!-- /figure -->

## At a glance

<!-- scaffold:readme-glance -->
| | |
| --- | --- |
| **Chip** | XMPL100 digital temperature sensor, 1.8 V to 3.6 V |
| **Measures** | Die temperature, -40 °C to +125 °C, 16-bit result at 1/128 °C per count |
| **Bus** | SPI mode 0 or 3, up to 10 MHz; 7-bit register addresses with auto-increment |
| **Also drives** | The open-drain ALERT pin (high and low thresholds); RESET_N, when your port has `GpioSet()` |
| **Driver** | Header-only C++20; no heap, exceptions, RTTI or virtual calls; every call returns `Result<T>` |
| **Runs on** | Any MCU with SPI and a C++20 compiler; the host example runs on any PC ([Platforms](docs/platforms.md)) |
| **Status** | 0.1.0, not yet verified on hardware: host-tested against a register model of the chip |
<!-- /scaffold:readme-glance -->

## Datasheet

<!-- generated: datasheet-card -->
| Document | Revision | Get it |
| --- | --- | --- |
| Datasheet: XMPL100 Digital Temperature Sensor (Example Corp (fictional) XMPL100-DS) | A, 2026-11-06 | [vendor page](https://example.com/xmpl100) |

Written against these revisions; the SHA-256 of each file is in [`docs/datasheets.yml`](docs/datasheets.yml). Where each
part of the driver is in the datasheet: [Datasheet map](docs/datasheet.md).
<!-- /generated -->

Every docs page lists the datasheet sections it relies on, and code comments cite the section they implement.

## Try it on your PC

No hardware needed: [`examples/host`](examples/host/README.md) runs the driver against a simulated chip.

```bash
cmake -S examples/host -B build-example && cmake --build build-example && ./build-example/xmpl100_host_example
```

## Use it on your MCU

**1. Wire it** ([Hardware](docs/hardware.md) has every pin, the timing and the power):

<!-- scaffold:bus-wiring -->
| MCU | XMPL100 | Notes |
| --- | --- | --- |
| SPI SCLK | SCLK | Mode 0 or 3, up to 10 MHz; 1 MHz for bring-up on jumper wires |
| SPI MOSI | SDI | |
| SPI MISO | SDO | High impedance while CS_N is high, so the bus can be shared |
| GPIO (chip select) | CS_N | Low for the whole frame: one transfer per frame |
| 3.3 V | VDD | 1.8 V to 3.6 V; 100 nF to GND close to the pin |
| GND | GND | |
<!-- /scaffold:bus-wiring -->

<!-- scaffold:readme-port -->
**2. Write the port**: a class with `EnsureInitialized()` (bring the bus up), `Transfer()` (one full-duplex frame
with CS_N low) and `DelayUs()`, about 20 lines on most SDKs. [Porting](docs/porting.md) has the contract and a
complete example.
<!-- /scaffold:readme-port -->

**3. Read the temperature:**

<!-- generated: quickstart-code -->
```cpp
#include "xmpl100.hpp"

int read_board_temperature(MyPort& port, float& celsius) {
  xmpl100::XMPL100<MyPort> sensor(port); // no I/O yet

  if (auto r = sensor.EnsureInitialized(); !r) { // 1. bus up, reset, CHIP_ID, Config{}
    return static_cast<int>(r.error); //    Bus, Timeout or DeviceIdMismatch: check wiring
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

`EnsureInitialized()` is the only call that starts the chip. Every other call returns `Error::NotInitialized`
until it has succeeded, and no read ever starts the chip on its own.

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
Details: [Installation](docs/installation.md) and [Platforms](docs/platforms.md).

## Documentation

| Page | Read it when you want to |
| --- | --- |
| [Quickstart](docs/quickstart.md) | Go from a bare chip to a temperature on your MCU |
| [Hardware](docs/hardware.md) | Wire it, check the bus settings, the timing and the power |
| [Porting](docs/porting.md) | Write the port class for your MCU's SDK |
| [API guide](docs/api-guide.md) | Use each call: start, configure, measure, set alerts, handle errors |
| [Configuration](docs/configuration.md) | Choose every setting, or start from a preset |
| [Troubleshooting](docs/troubleshooting.md) | Find the cause of an error or a wrong reading |
| [Contract](docs/contract.md) | Know exactly what the driver promises, needs and returns |
| [Datasheet map](docs/datasheet.md) | Find the datasheet section behind each part of the driver |
| [API reference](https://kokeb-labs.github.io/driver-template/latest/api/) | Look up a class or member (Doxygen) |

## Tests and quality

```bash
cmake -S . -B build -DXMPL100_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build
```

The host tests run the driver against a register model of the chip, fail every bus transfer in turn, and produce
each error the driver can return. CI also builds for Cortex-M and checks coverage, complexity, sanitizers,
clang-tidy and the handbook rules (`.kokeb/handbook.yml`). See [Testing and CI](docs/testing.md).

## License

MIT. See [LICENSE](LICENSE).
