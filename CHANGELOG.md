# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Breaking

- I2C overlay: `kDefaultI2cAddress` is now `kI2cAddress` (handbook KB-CPP-123).

### Added

- Bus requirements as `constexpr` in the bus interface header (handbook KB-CPP-123): `kSpiMode`, `kMsbFirst`,
  `kMaxClockHz`, `kMinCsHighNs`, `kCsSetupNs`, `kCsHoldNs` and `kContinuousWord` for SPI; `kI2cAddress`,
  `kMaxClockHz` and `kRepeatedStart` for I2C; `kMaxBaudRate` and `kTurnaroundUs` for UART. Both example ports
  `static_assert` their settings against them, the ESP32 SPI port also checks its run-time clock and mode, and the
  Porting page lists them.
- Diagnostics (handbook KB-CPP-119, KB-CPP-129): `ReadRegister()` (one transfer, no retry), `GetLastStatusByte()`,
  and a latch for STATUS.POR fed by every STATUS read the driver makes, internal ones included
  (`GetLatchedStatus()`, `ClearLatchedStatus()`); the start-up empties it. The ADC and GPIO overlays get
  `ReadRawVoltageMv()` / `GetLastVoltageMv()` and `ReadRawOutput()` / `GetLastOutput()`.
- `tests/test_model.cpp`: the chip model's register map, reset values, timing and CRC-8 against the driver's, with
  `static_assert` (handbook KB-TST-030). The Testing page explains the pattern.
- A `Handbook source checks` job in `ci-repo-checks.yml`: the handbook's `checks/source_checks.py` (include guards,
  bus requirements, raw access) from a pinned handbook commit.
- The Porting page lists each control pin with its idle level; the port drives it there before it enables the
  output (handbook KB-HW-018). `docs/template.md` lists which handbook rules the example chip shows and which are
  the driver author's.

- Template drift check: the scaffold records the template commit and its arguments in `.kokeb/template.yml`, and
  `tools/scaffold.py --check-drift DIR [--fix]` scaffolds the template again with them and compares the files the
  template manages (workflows, `_config/` except `Kconfig`, skills, CMake helpers). A file that differs on purpose
  is listed under `local` with its reason. CI runs it as the `Template drift` job (ci-tools `ru-template-drift`,
  warning only), and the self-test checks that a fresh scaffold has no drift.
- `release.yml`: the `attest` job signs build provenance for every release file (ci-tools `ru-attest-release`); a
  private repository skips it.

### Changed

- Every header, generated ones and the scaffold's overlays included, has an `#ifndef` include guard named
  `<NAME>_<FILE_STEM>_<EXT>` instead of `#pragma once` (handbook KB-CPP-019, MISRA C++:2023 Rule 19.2.1). The
  scaffold renames the guards with the chip and the bus, and its self-test checks every guard of the template and
  of each scaffolded repository.
- `_config/.clang-tidy` follows the handbook's: `hicpp-signed-bitwise`, `readability-uppercase-literal-suffix`
  (L, LL, UL, ULL), `misc-no-recursion`, `misc-static-assert`, `misc-definitions-in-headers`,
  `misc-unconventional-assign-operator`, `hicpp-no-assembler`, `hicpp-multiway-paths-covered` and
  `fuchsia-virtual-inheritance` are on; the exemptions for union access (there are no unions), varargs (the `Log`
  hook takes a plain string) and `#pragma once` are gone. The host tests and the host example turn
  `cppcoreguidelines-pro-type-vararg` off in their own folders, for their `std::printf` output.
- Narrow unsigned operands are cast to `uint32_t` before `<<`, `~`, `|` and `^` (MISRA C++:2023 Rules 7.0.4,
  7.0.5), in the driver, the model, the tests and the overlays.
- `tests/mock_xmpl100.hpp` no longer includes `xmpl100_registers.hpp` or `xmpl100_types.hpp`: it carries its own
  register map, reset values, timing and CRC-8 (namespace `xmpl100_test::chip`) with datasheet sections. The UART
  overlay's bus mock uses the model's CRC.
- The ESP32 ports set RESET_N high, or SHDN_N low, before they enable the pin's output.

- No workflow has a `workflow_dispatch` trigger any more (ci-tools trigger policy): a manual run is a re-run. A
  dispatched run executes in the default branch's context and can write the cache that `main` restores. The ESP32
  build's `clean_build` input went with it; `gh cache delete --all` empties the caches when a clean build is wanted.

- CI runs fewer jobs, because a private repository is billed every job rounded up to a whole minute: host tests
  build GCC and Clang in one job; DCO, the org standard, Markdown and YAML lint, the handbook rules, template drift
  and cppcheck are steps of one `Repository checks` job (`ci-repo-checks.yml`, ci-tools `ru-repo-checks`), which
  replaces `ci-handbook.yml`, `ci-cpp-analysis.yml`, `ci-markdown-lint.yml` and `ci-yaml-lint.yml`; the
  cross-compile and host-quality checks use ci-tools 1.11's one-job layouts. No check is dropped, and the gate no
  longer needs `allow_skipped` (DCO passes with nothing to check outside a pull request).
- `AGENTS.md` states that extra read-back and verification are chosen per call with a `Verify` argument that
  defaults to `Verify::None` (handbook KB-ARCH-041).

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
