# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Added

- `.kokeb/handbook.yml` declares the handbook release, kind and assurance level (high), and `driver.yml` the
  driver's metadata (name, version, bus, MCUs, peripheral, minimum kb-core). The scaffold writes the generated
  repository's kind and bus into them; a library has no `driver.yml`.
- CI checks the handbook rules a script can decide (ci-tools `ru-handbook-check`) and compiles every test source,
  and so every operation of the driver, for Cortex-M0+, M4F and M7F (`ru-cross-build`); both are in the CI gate.

### Fixed

- clang-format checks `src/<name>.ipp`: the lint job's `extensions` include `ipp` (ci-tools 1.5.1 also measures
  its complexity).
- The hook tables in the Doxygen comments of the driver header and every port header keep one row per line:
  clang-format had reflowed each table into a single paragraph, which Doxygen does not render as a table. The
  comment blocks are wrapped in `// clang-format off` / `// clang-format on`.

### Breaking

- `EnsureInitialized()` is the only way to start the driver. The constructor takes the configuration,
  `XMPL100(port, config = {})` (after the bus argument, such as the I2C address, where there is one), stores it and
  does no I/O; `Initialize(config)` is gone (now private). The first `EnsureInitialized()` checks the stored
  configuration, brings the bus up, resets the chip, checks CHIP_ID and writes the configuration; later calls return
  `Ok` with no I/O, and after a failure the next call starts again from the beginning. Recovery after a fault is
  `Deinitialize()`, then `EnsureInitialized()`. `ApplyConfig()`, `SetMode()` and `SetAlertThresholdsC()` before the
  start store the configuration with no I/O instead of returning `NotInitialized`; no read or write starts the chip
  as a side effect. Replace `sensor.Initialize(c)` with `XMPL100 sensor(port, c);` and `sensor.EnsureInitialized()`.

### Added

- XMPL100 driver: lifecycle with the chip-ID check, continuous and one-shot conversions, CRC-checked reads, alert
  thresholds, `Result<T>` errors, `Config` with datasheet defaults and three presets, optional `DelayUs`,
  `GpioSet` and `Log` port hooks.
- Host tests on a register model, compiled documentation snippets, an install test and an ESP32-C6 example.
- `examples/host/`: the driver on any PC against the register model, through a host port; built and run by the
  host tests and on its own in CI, and the quickstart's first step.
- `tools/setup.py` and `tools/scaffold.py`: turn the template into a driver (SPI, I2C, UART, ADC or GPIO only) or a
  library, with the organisation, repository, author and licence (MIT or Apache-2.0) as options.
