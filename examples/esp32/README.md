# XMPL100 ESP32 examples

ESP-IDF applications that run the XMPL100 driver on an ESP32-C6, built by CI on every pull request.

<!-- scaffold:bus-esp32-wiring -->
## Wiring (ESP32-C6 DevKitC-1)

| ESP32-C6 | XMPL100 | Notes |
| --- | --- | --- |
| GPIO6 | SCLK | SPI2, mode 0, 1 MHz in the example (up to 10 MHz) |
| GPIO7 | SDI (MOSI) | |
| GPIO2 | SDO (MISO) | |
| GPIO10 | CS_N | Driven by spi_master, one transfer per frame |
| GPIO11 | RESET_N | Optional; set `reset_n = -1` in `Esp32Xmpl100Pins` when tied high |
| 3V3 | VDD | 100 nF to GND at the pin |
| GND | GND | |
<!-- /scaffold:bus-esp32-wiring -->

## Build and flash

```bash
git submodule update --init scripts      # the ESP-IDF build scripts
. $IDF_PATH/export.sh                     # ESP-IDF v5.5
./scripts/build_app.sh list
./scripts/build_app.sh xmpl100_basic_example Debug
./scripts/flash_app.sh flash_monitor xmpl100_basic_example Debug
```

`idf.py menuconfig` shows "XMPL100 temperature sensor" under Component config: the output data rate,
averaging, CRC and the one-shot timeout that `xmpl100::KconfigDefaults()` returns.

## Expected output

```text
I (312) xmpl100_example: XMPL100 driver 0.1.0
xmpl100: initialised
I (318) xmpl100_example: initialised, silicon revision 0x20
I (820) xmpl100_example: 23.45 C
```

An `Initialize failed: DeviceIdMismatch` line means CHIP_ID did not read 0xA1: check CS, the SPI mode and
the MISO wire first.

## Files

| File | What it is |
| --- | --- |
| [`main/xmpl100_basic_example.cpp`](main/xmpl100_basic_example.cpp) | The example app |
| [`main/esp32_xmpl100_port.hpp`](main/esp32_xmpl100_port.hpp) | The ESP-IDF port: bus, RESET_N, delay, log |
| [`CMakeLists.txt`](CMakeLists.txt) | The ESP-IDF project; `EXTRA_COMPONENT_DIRS` points at the repository root, which is the component |
| [`app_config.yml`](app_config.yml) | The app matrix for the scripts, CI and the docs |
