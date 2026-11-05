# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Breaking

- None yet.

### Added

- XMPL100 driver: lifecycle with the chip-ID check, continuous and one-shot conversions, CRC-checked reads, alert
  thresholds, `Result<T>` errors, `Config` with datasheet defaults and three presets, optional `DelayUs`,
  `GpioSet` and `Log` port hooks.
- Host tests on a register model, compiled documentation snippets, an install test and an ESP32-C6 example.
- `examples/host/`: the driver on any PC against the register model, through a host port; built and run by the
  host tests and on its own in CI, and the quickstart's first step.
- `tools/setup.py` and `tools/scaffold.py`: turn the template into a driver (SPI, I2C, UART, ADC or GPIO only) or a
  library, with the organisation, repository, author and licence (MIT or Apache-2.0) as options.
