# Testing and CI

How the driver is checked: host tests against a model of the chip, a fuzzer that corrupts what the chip sends
back, compiled documentation, an install test, ESP32 and Cortex-M builds, static analysis and measured quality
gates, on every pull request.

## Host tests

The tests build the real driver against a register model of the XMPL100: reset values, SOFT_RESET holding
STATUS.BUSY, one-shot conversions that return to shutdown, DRDY and POR clearing, the CRC byte, OFFSET, and
injected faults (bus errors, wrong CHIP_ID, stuck reset, stuck conversion, sensor fault, corrupted CRC, a
brown-out reset). They build
with the firmware's flags, `-fno-exceptions -fno-rtti`, and warnings as errors, on GCC and Clang.

```bash
cmake -S . -B build -DXMPL100_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

| File | Covers |
| --- | --- |
| [`tests/test_xmpl100.cpp`](../tests/test_xmpl100.cpp) | Lifecycle, configuration, measurement, optional hooks, bus errors |
| [`tests/fuzz/`](../tests/fuzz/fuzz_xmpl100.cpp) | The fuzz target; CTest replays its corpus (`xmpl100_fuzz_xmpl100_corpus`), see below |
| [`tests/test_errors.cpp`](../tests/test_errors.cpp) | The base `Error` values are 0 to 9; every chip-specific value (16 and up) is returned by some call |
| [`tests/test_version.cpp`](../tests/test_version.cpp) | The version getters follow the generated header (also against a 9.8.7 probe) |
| [`tests/test_kconfig.cpp`](../tests/test_kconfig.cpp) | `KconfigDefaults()` with and without `CONFIG_` values |
| [`tests/compile_fail/`](../tests/compile_fail/port_without_delay.cpp) | Code that must not compile, each for its `EXPECT:` diagnostic: a port without `DelayUs()`, a `void GpioSet()`, a `DelayUs(uint16_t)`, a hook with a narrowed parameter or without `noexcept`, an invalid `CheckedConfig`, a class that is not a port |
| [`tests/mock_xmpl100.hpp`](../tests/mock_xmpl100.hpp) | The register model, with its own register map; the bus framing is in `mock_xmpl100_bus.hpp` |
| [`tests/test_model.cpp`](../tests/test_model.cpp) | The model's register map, reset values, timing and CRC against the driver's, with `static_assert` |
| [`tests/check_version_copy.cmake`](../tests/check_version_copy.cmake) | `_config/Doxyfile` carries the CMake version |
| [`examples/host/`](../examples/host/README.md) | The host example, built with the test flags and run (`xmpl100_host_example`) |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | Installs into a scratch prefix and builds [`tests/install_consumer/`](../tests/install_consumer/CMakeLists.txt) with `find_package(xmpl100)` |
| [`tests/check_doxygen.cmake`](../tests/check_doxygen.cmake) | Doxygen with `_config/Doxyfile` and warnings as errors, when `doxygen` is installed |

### The model stands alone

[`tests/mock_xmpl100.hpp`](../tests/mock_xmpl100.hpp) is written from the datasheet, not from the driver: it does
not include `xmpl100_registers.hpp` or `xmpl100_types.hpp`. Its register map, field masks, reset values, timing and
CRC-8 are its own (namespace `xmpl100_test::chip`, each with the datasheet section), so a wrong constant in the
driver cannot hide behind the same wrong constant in the model (handbook KB-TST-030).
[`tests/test_model.cpp`](../tests/test_model.cpp) compares the two at compile time:

```cpp
--8<-- "tests/test_model.cpp:cross-check"
```

When the build stops on one of these, one of the two maps is wrong: check it against the section the model names,
and fix that one. A new register goes into both, with its check. Every behaviour found on hardware becomes a fault
mode of the model and a test that fails on the code before the fix; a chip with timing gets a virtual clock in the
model (here `Elapse()`, driven by the mock port's `DelayUs()`).

## Fuzz tests

Everything the driver reads comes from the chip over the bus, so the data read is what a noisy cable, a wrong part
or a broken chip controls. [`tests/fuzz/fuzz_xmpl100.cpp`](../tests/fuzz/fuzz_xmpl100.cpp) lets libFuzzer choose a
configuration, then up to 64 driver calls, and on every bus transfer what the bus does: pass it to the model, fail
it, flip bits of the data read or replace it (and, on a bus that acknowledges writes, lose an acknowledged write).
Between calls it can change the chip: a wrong CHIP_ID, revision A silicon, a sensor fault, a stuck reset or
conversion, a corrupted CRC, any temperature, a brown-out. After every call it checks the driver's promises and
aborts, naming the line, when one breaks:

- a failed call returns an `Error` the enum defines and `T{}`; a temperature is inside the range of the 16-bit
  result;
- before `EnsureInitialized()` no call makes a transfer, and `InvalidArgument` means nothing was sent;
- a started driver with CRC enabled runs on revision B silicon, and `UnexpectedReset` leaves it uninitialised;
- every log line is a non-empty plain-text message.

When the model becomes your chip's, keep the target and turn its checks into your chip's promises: every value range
the datasheet defines, and every state the driver must never reach.

```bash
# libFuzzer needs Clang (on macOS, Homebrew's LLVM)
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DXMPL100_BUILD_TESTS=ON -DXMPL100_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_xmpl100
./build-fuzz/tests/fuzz/fuzz_xmpl100 -max_total_time=120 tests/fuzz/corpus/fuzz_xmpl100
```

The same file also builds without libFuzzer as `fuzz_replay_fuzz_xmpl100`, and CTest replays every input in
[`tests/fuzz/corpus/fuzz_xmpl100/`](../tests/fuzz/corpus/fuzz_xmpl100) on GCC and Clang
(`xmpl100_fuzz_xmpl100_corpus`): the seeds, and every input that ever broke a promise (name it `regress-*`), so a
fixed bug stays fixed. CI fuzzes for 120 s on every pull request, from the seeds plus a corpus it keeps between runs
(job `Host quality / fuzz`).

## Quality gates

The `Host quality` job measures the library code, `inc/` and `src/` only (tests and examples are left out), and
fails when a value is over the limits of this repository's assurance level, `high` (set in
[`.kokeb/handbook.yml`](../.kokeb/handbook.yml), defined by the handbook's ADR-0020):

| Gate | Tool and how it is measured | Limit at `high` |
| --- | --- | --- |
| Complexity | lizard 1.24.1, modified cyclomatic complexity (`-m`: a `switch` counts once) per function | CCN at most 15 |
| Function length | lizard, lines of code per function | at most 60 |
| Parameters | lizard, per function | at most 6 |
| Line coverage | GCC 14 `--coverage -O0`, every CTest, gcovr 8.7 `--merge-lines` (a template's lines count once across its instances), `--exclude-unreachable-branches --exclude-throw-branches` | at least 85 % |
| Branch coverage | same run | at least 75 % |
| MC/DC | GCC `-fcondition-coverage`, same run | measured, not gated at `high` (100 % at `safety`) |
| Sanitizers | Clang, `-fsanitize=address,undefined -fno-sanitize-recover=all`, every CTest | no report |
| Fuzzing | libFuzzer with ASan and UBSan, 120 s per target | no broken promise |

The reports are in the run's `host-quality-reports` artifact and its step summary. To run the gates locally:

```bash
# Coverage (GCC 14 or later), then the complexity limits
cmake -S . -B build-cov -DCMAKE_CXX_COMPILER=g++-14 -DXMPL100_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -fcondition-coverage"
cmake --build build-cov && ctest --test-dir build-cov
gcovr --root . --filter inc/ --filter src/ --merge-lines --exclude-unreachable-branches --txt build-cov
lizard -m -C 15 -L 60 -a 6 inc src

# Sanitizers (Clang)
cmake -S . -B build-san -DCMAKE_CXX_COMPILER=clang++ -DXMPL100_BUILD_TESTS=ON \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g"
cmake --build build-san && ctest --test-dir build-san --output-on-failure
```

The fuzz gate runs locally as shown in [Fuzz tests](#fuzz-tests).

clang-tidy 20 ([`_config/.clang-tidy`](../_config/.clang-tidy): bugprone, CERT, clang-analyzer,
cppcoreguidelines, modernize, performance, portability and readability, each disabled check with its reason) and
cppcheck (strict) run in the `C++ lint` and `Repository checks` jobs and fail on any finding.

The `MISRA` job ([`ci-misra.yml`](../.github/workflows/ci-misra.yml)) runs the shared `ru-misra` workflow of
ci-tools at level `high`: it traces a host build and the Cortex-M build (`xmpl100_optimised_build`) of every
translation unit, checks them against MISRA C++:2023 and the handbook's C++20 profile (the CodeQL coding-standards
and profile packs, plus the validated clang-tidy and cppcheck checks), and reports each finding against the
handbook's compliance matrix and deviation records. A finding is fixed in the code; a deliberate exception names
its deviation on the line before it (`// codeql::misra_deviation_next_line(MD-CPP-009)` above the `.ipp` include
of `inc/xmpl100.hpp`). Until the repository is public the job runs with `open_source_prerelease: true`, the
declaration that the code is released under the licence in `LICENSE` and is to be published.

## Documentation snippets

Every C++ block on this site is included from [`tests/docs/`](../tests/docs/my_port.hpp), which the host build
compiles (target `xmpl100_doc_snippets`) with the driver instantiated over the documented port. A change to
the API that breaks a page breaks the build first. The README's example is a generated copy of the same file;
`kokeb-docs check` fails when it drifts.

## CI workflows

Everything runs from [`ci.yml`](../.github/workflows/ci.yml), which calls one workflow file per check and ends in
the `CI gate` job, the single required check (`CI gate / gate`).

| Job | What it checks |
| --- | --- |
| Host tests | GCC, then Clang (one job), build and run every test above, then build and run `examples/host` on its own |
| Host quality | The gates above: complexity, coverage, sanitizers, fuzzing |
| Cross-compile | `arm-none-eabi-g++` for Cortex-M0+, M4F and M7F with the strict flags, warnings as errors (nothing linked) |
| C++ lint | clang-format and clang-tidy over the sources |
| MISRA | MISRA C++:2023 and the C++20 profile on traced host and Cortex-M builds, at level `high` (see [Quality gates](#quality-gates)) |
| Docs | Strict site build (`_config/mkdocs.yml`), figure and generated-region drift, Doxygen (`_config/Doxyfile`), offline link check, Lighthouse |
| Repository checks | One job: DCO on pull requests, the org standard, Markdown and YAML lint, the [Kokeb Labs handbook](https://github.com/kokeb-labs/handbook) rules for `.kokeb/handbook.yml` (by rule ID), template drift (warns only) and cppcheck over the driver sources; a second job runs the handbook's source checks (include guards, bus requirements, raw access) |
| CI gate | Passes only if every job above passed: the one required check |
<!-- scaffold:platform-esp32 -->

The **ESP32 examples** job builds every app in `examples/esp32/app_config.yml`, for each listed ESP-IDF version and
build type, with the repository root as the driver's component, and lints the example sources. The gate needs it
too.
<!-- /scaffold:platform-esp32 -->

The workflows call the shared, versioned workflows of
[kokeb-labs/ci-tools](https://github.com/kokeb-labs/ci-tools) at `@v1`.

## On hardware

An MCU example ([Examples](examples.md)) is the bench test: it logs the temperature twice a second. Warm the chip
and watch the value rise. Record the board and the result in the pull request's bench-status section.
