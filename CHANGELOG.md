# Changelog

All notable changes to this repository are listed here. Versions follow [Semantic Versioning](https://semver.org/).
Entries under **Breaking** need source changes in code that used the affected API; any of them calls for a major
version bump (a minor bump before 1.0).

## [Unreleased]

### Breaking

- The exact port concepts match each hook's address with `detail::ExactHook<Port, Sig>` instead of probing it
  with `detail::Exactly<T>`, which needed an implicit conversion operator (MISRA C++:2023 Rule 15.1.3). A hook
  must be a member function, `const` or not, of exactly the documented signature; `detail::Exactly` is gone.
- `detail::kAlwaysFalse` takes a class type (`ClassType`) and `detail::EnumLast`, `ToUnderlying` and
  `IsEnumerator` an enumeration (`Enumeration`) (KB-CPP20-005), each pinned by a compile-fail test.
- The copyright holder is the repository's authors: `The <repo> authors` (`The driver-template authors` here)
  in `LICENSE`, every source header, `driver.yml` (new `authors` field) and the docs footer (`_config/mkdocs.yml`
  `copyright`). The scaffold's `--author` default is now `The <repo> authors`; `--author` still overrides it.
- I2C overlay: `kDefaultI2cAddress` is now `kI2cAddress` (handbook KB-CPP-123).
- Port concepts check each hook's exact signature and `noexcept` (handbook KB-CPP20-006): a hook with a narrower
  or different parameter type, or without `noexcept`, no longer satisfies `SpiPort` (and the I2C, UART, ADC and
  GPIO concepts). A misdeclared `DelayUs` or `Log` hook is now a compile error, as `GpioSet` already was.
- `Validate()` rejects `conversion_timeout_ms` above `kConversionTimeoutMaxMs` (10000, the Kconfig range), and
  `WaitDataReady()` is bounded by it: an unvalidated `UINT32_MAX` timeout used to make the poll loop endless.
- `reg::Crc8()` and the register bus helpers take `std::span` instead of a pointer and a length; the bus helpers
  also have fixed-extent overloads that check the burst size at compile time. `GetConfig()` is `&`-qualified
  (deleted on a temporary driver).

### Added

- `tools/preflight.sh`: every check CI runs that a workstation can run, in one command, with a list of what it
  skipped; the `verify` skill and AGENTS.md ask for it before every push. Scaffolded repositories keep it as a
  template-managed file.
- `hardware-lessons-review` rules 12a to 12d, 22 and 23: a device reset is detected and cached state is never
  written over it (a pending reset check holds back every value), a fault the chip clears itself still reaches
  the latch, a write the chip may have dropped is not Ok, no uncertainty refuses a switch-off, a frame with no
  integrity check never decides a state change alone, the model changes state on its own and offers both
  readings of an ambiguous datasheet, fuzz invariants are weakened rather than deleted, and an independent review
  comes before the first release. The `verify` skill notes the macOS libFuzzer link and coverage differences.
- CI job `MISRA` (`.github/workflows/ci-misra.yml`): the shared `ru-misra` workflow at level `high`, MISRA
  C++:2023 and the C++20 profile on traced host and Cortex-M builds, with `open_source_prerelease` until the
  repository is public. The clang-tidy `cppcoreguidelines-macro-usage` check allows `CONFIG_` macros, so the
  Kconfig headers carry no suppression.
- `detail::operator""_u8`, `_u16` and `_u32`: `consteval` literals of exactly `uint8_t`, `uint16_t` and
  `uint32_t`, for constant arguments (Rule 7.0.6); a value that does not fit does not compile.
- `tools/check_standalone.py`, run in the repository checks and kept in every scaffolded repository by the drift
  check: a repository names no other project, carries no dated history and gives its copyright to its authors.
  It also reports a private repository, a personal account outside `.github/CODEOWNERS` and a local path.
  `scaffold.py --selftest` runs it with `--strict` on the template and on every scaffolded case.
- The `misra-review` skill names no private repository: it uses a licensed copy of the standard and the
  maintainers' private notes only when the reviewer holds them, and the handbook's matrix otherwise.
- `AGENTS.md`: sections on reading and citing the datasheet, the standards, branches and pull requests (squash
  merge once green), the definition of done and the safety rules.
- Issue and PR templates ask for the board, MCU and port in one free-text field.
- The handbook's C++20 profile in the template (KB-CPP20-001 to 058):
  - a strict-dialect build: `CMAKE_CXX_EXTENSIONS OFF` in the root, `tests/`, `examples/host/` and the install
    consumer, `-pedantic-errors` and the C++20 deprecation warnings as errors, and `xmpl100_header_checks`, one
    generated translation unit per public header asserting `__cplusplus == 202002L`, built by the host tests (GCC,
    Clang) and, through `xmpl100_optimised_build`, the Cortex-M cross-build. The ESP-IDF example compiles
    `main/xmpl100_cxx20_check.cpp` at `-std=c++20 -pedantic-errors` inside the ESP-IDF build, whose own dialect is
    newer;
  - `inc/xmpl100_detail.hpp`: `detail::Exactly<T>`, `kAlwaysFalse`, `ToUnderlying`, `IsEnumerator`/`EnumLast`
    and typed register fields `detail::Field`; named `Declares*`/`Has*` concepts for every optional hook;
  - `enum class [[nodiscard]] Error` in every overlay; masked shift counts in `SamplesPerResult()` and
    `PeriodUs()`; the register encoders and the STATUS decoder as free `constexpr` functions;
  - `Resolution` with `ToCounts()` (rejects NaN and out-of-range values) and `ToCelsius()`; `CheckedConfig`, a
    configuration checked at build time;
  - `static_assert`s: every preset valid, the reset `Config` encodes to the CTRL and CONFIG reset values, fields
    round-trip, the CRC-8 matches its check value, the threshold reset values convert both ways;
  - compile-fail tests for a narrowed hook parameter, a hook without `noexcept`, a `DelayUs(uint16_t)` and an
    invalid `CheckedConfig`; unit tests for the bounded timeout, the masked helpers, the conversions, the fields;
  - the standard library's assertions (`_GLIBCXX_ASSERTIONS`, libc++ hardening) in the tests and fuzzers.
- Review skills and an agent, shipped with every scaffolded repository: `cpp20-profile` (the KB-CPP20 guidelines
  with the check for each), `misra-review` (the pinned handbook's MISRA C++:2023 matrix, numbers only, deviations
  the handbook's way), `hardware-lessons-review` (drivers: the rules learnt on hardware), and
  `.claude/agents/safety-reviewer.md`, a read-only reviewer that runs them with `datasheet-conformance` and ranks
  findings by severity. `verify`, `add-driver-feature` and `new-driver-from-template` call them and run the
  strict-dialect build and the source checks. `.claude/agents/` is part of the template drift check.
- The source checks job runs the handbook at its C++20-profile commit; `_config/.clang-tidy` follows the
  handbook (`bugprone-dangling-handle` watches `std::span`; `modernize-use-std-format`/`-print` off).
- `docs/template.md`: the C++20 patterns the template uses and the ones a driver adopts when its chip needs them.

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

- `hardware-lessons-review` rule 12c: a channel in doubt counts as on, for a "nothing changed" shortcut too; a
  refused switch-on in a group of unknown mode leaves the group off; a failed read-back keeps the channels in
  doubt. Rule 22: a fuzz invariant is skipped only for the call whose own fault the driver cannot see, and is
  checked again after a successful `EnsureInitialized()`.
- The code follows the first MISRA C++:2023 scan, in the driver and in every scaffold overlay: every switch over
  an enumeration ends with `default: break;` (Rule 9.4.2) and the tests build with `-Wswitch-enum`, so a new
  enumerator still warns; every `else if` chain ends with `else` (Rule 9.4.1); operands of a different precedence
  are parenthesised (Rule 8.0.1); narrow operands meet `U` constants or are widened to `uint32_t` first
  (Rule 7.0.5); constant arguments have their parameter's type (Rule 7.0.6); loop bounds are `const` locals
  (Rule 9.5.1); and the `.ipp` include of the umbrella header names deviation MD-CPP-009 (Rule 19.0.3).
  Behaviour is unchanged.
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
