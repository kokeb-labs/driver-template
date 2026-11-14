| | |
| --- | --- |
| **Measures** | Die temperature, -40 °C to +125 °C, 16-bit result at 1/128 °C per count |
| **Bus** | UART at 115200 baud, 8N1; framed register access with a CRC-8 on every frame |
| **Also drives** | The open-drain ALERT pin (high and low thresholds); RESET_N, when your port has `GpioSet()` |
| **Driver** | Header-only C++20; no heap, exceptions, RTTI or virtual calls; every call returns `Result<T>` |
| **Runs on** | Any MCU with a UART and a C++20 compiler; the host example runs on any PC ([Platforms](docs/platforms.md)) |
| **Status** | 0.1.0, not yet verified on hardware: host-tested against a register model of the chip |
