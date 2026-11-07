# Quickstart

Run the driver on your PC first, with no hardware; then read the temperature from your MCU: wire the chip, write
a small port class, call two driver functions.

## Try it on your PC

[`examples/host/`](../examples/host/README.md) runs the real driver against a simulated XMPL100, through a host
port. From the repository root, with CMake and a C++20 compiler:

```bash
cmake -S examples/host -B build-example
cmake --build build-example
./build-example/xmpl100_host_example
```

```text
XMPL100 driver 0.1.0 on the host
xmpl100: initialised
25.00 C
```

The calls in [`main.cpp`](../examples/host/main.cpp) are the ones below; only the port differs on an MCU. CI builds
and runs this example on every pull request.

## 1. Wire it

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

RESET_N and ALERT are optional: tie RESET_N high and leave ALERT open for this example. The
[Hardware](hardware.md) page has the full pin list and the timing.

## 2. Add the driver

Add the repository as a submodule and link the `xmpl100::xmpl100` CMake target. [Installation](installation.md)
covers CMake on any MCU, the installed package and ESP-IDF.

```bash
git submodule add https://github.com/kokeb-labs/driver-template.git external/driver-template
```

## 3. Write the port

The port is the only platform code: it brings the bus up and moves bytes. Replace the `platform_*` calls with your
SDK's.

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

[Porting](porting.md) explains each method, the optional hooks and the transfer rules.

## 4. Read the temperature

```cpp title="main.cpp"
--8<-- "tests/docs/quickstart.cpp:quickstart"
```

What each step does:

1. `EnsureInitialized()` calls your port's `EnsureInitialized()`, soft-resets the chip, waits for it, checks that
   CHIP_ID reads `0xA1` and writes `Config{}`, the datasheet reset values: continuous conversions at 4 Hz.
2. `ReadTemperatureC()` checks STATUS for a sensor fault, then reads the latest result in one burst.
3. The value is in degrees Celsius with 1/128 °C resolution.

This example is built by the host CI, so it compiles against the current headers.

## If it does not work

| Symptom | First thing to check |
| --- | --- |
| `EnsureInitialized()` returns `Bus` | Your port's `EnsureInitialized()`: the bus did not come up |
| `EnsureInitialized()` returns `DeviceIdMismatch` | The bus wiring ([Troubleshooting](troubleshooting.md#ensureinitialized-returns-deviceidmismatch)) |
| `EnsureInitialized()` returns `Timeout` | VDD and RESET_N: the chip never finished its reset |
| `ReadTemperatureC()` returns `DeviceFault` | The sensing element is damaged: replace the part |

## Next

- [API guide](api-guide.md): one-shot measurements, alerts, errors.
- [Configuration](configuration.md): presets, every `Config` field, Kconfig.
- [Examples](examples.md): the host example and the MCU examples.
- [Platforms](platforms.md): which MCUs have a port and an example.
