# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Fixed

- The scaffold ends the description with a full stop in `AGENTS.md`, where the next sentence follows it
  (driver-at25040b read "EEPROMs It follows").

### Added

- A fuzz scaffold in `tests/fuzz/`: `fuzz_<chip>.cpp` runs the driver over its host-test model while the fuzzer
  passes, fails, corrupts or replaces every transfer, and checks the driver's promises after each call (known
  `Error` values, `T{}` on failure, no transfer before `EnsureInitialized()`, nothing sent on `InvalidArgument`,
  value ranges). Its corpus replays as a CTest on every compiler; `-D<CHIP>_BUILD_FUZZERS=ON` builds the libFuzzer
  target, and `Host quality` fuzzes it on every pull request (ci-tools 1.6.0 `fuzz_cmake_args`). The i2c and uart
  overlays swap in their bus hooks, adc and none fuzz the ADC reading and the OUT pin; a library has no untrusted
  input and gets no fuzz target.
- clang-tidy runs the CERT C++ checks (`cert-*`) as well; neither the template nor any scaffolded bus had a finding.
- `docs/testing.md`: a Fuzz tests section, a Quality gates section with every gate at level `high`, its limit, how
  it is measured and how to run it locally, and the Host quality, Cross-compile and Handbook check jobs in the CI
  table.
- `.kokeb/handbook.yml` declares the handbook release, kind and assurance level (high), and `driver.yml` the
  driver's metadata (name, version, bus, MCUs, peripheral, minimum kb-core). The scaffold writes the generated
  repository's kind and bus into them; a library has no `driver.yml`.
- CI checks the handbook rules a script can decide (ci-tools `ru-handbook-check`) and compiles every test source,
  and so every operation of the driver, for Cortex-M0+, M4F and M7F (`ru-cross-build`); both are in the CI gate.
- `docs/datasheets.yml` (handbook ADR-0019) records the datasheet the driver follows and every topic it takes from
  it; `tools/docs/generators/datasheet.py` builds from it the README and home-page card, a Datasheet map page and a
  box after the intro of each page that relies on the datasheet. The XMPL100's record is a fictional placeholder;
  `--datasheet` writes its `url`, and the checklist and the `new-driver-from-template` and `datasheet-conformance`
  skills say to fill in the rest. Every bus gets it; a library gets none of it.

### Changed

- The `Log()` hook is `void Log(LogLevel, const char* message) noexcept`: the message is plain text, and the
  examples print it with `puts` or `"%s"` instead of using it as a format string (no C variadic function, CERT
  DCL50-CPP; no format string from data). A port with the old variadic signature still compiles.
- README: "Try it on your PC" is numbered steps with the expected output; "Install" is "Add it to your project",
  with a picker table and the CMake, ESP-IDF (left out without the esp32 platform) and installed-package steps. The
  documentation link points to `docs/index.md`, and the API reference row to `docs/api-reference.md`.
- The README is a complete front door for every bus and the library: what the part is and what the driver gives,
  an At a glance table, the datasheet card, the wiring, the port and the first read, and a documentation table
  that says when to read each page. Links to docs pages are relative, so they work before the site is published.

### Fixed

- clang-format checks `src/<name>.ipp`: the lint job's `extensions` include `ipp` (ci-tools 1.5.1 also measures
  its complexity).
- The hook tables in the Doxygen comments of the driver header and every port header keep one row per line:
  clang-format had reflowed each table into a single paragraph, which Doxygen does not render as a table. The
  comment blocks are wrapped in `// clang-format off` / `// clang-format on`.
- The I2C and UART port headers of the scaffold build their frames in `std::array` instead of C arrays, which
  clang-tidy (`cppcoreguidelines-avoid-c-arrays`, `modernize-avoid-c-arrays`) rejected in a scaffolded driver.

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
