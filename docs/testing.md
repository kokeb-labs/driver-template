# Testing and CI

How the driver is checked: host tests against a model of the chip, compiled documentation, an install test,
ESP32 builds and static analysis, on every pull request.

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
| [`tests/test_errors.cpp`](../tests/test_errors.cpp) | The base `Error` values are 0 to 9; every chip-specific value (16 and up) is returned by some call |
| [`tests/test_version.cpp`](../tests/test_version.cpp) | The version getters follow the generated header (also against a 9.8.7 probe) |
| [`tests/test_kconfig.cpp`](../tests/test_kconfig.cpp) | `KconfigDefaults()` with and without `CONFIG_` values |
| [`tests/compile_fail/`](../tests/compile_fail/port_without_delay.cpp) | Code that must not compile, each for its `EXPECT:` diagnostic: a port without `DelayUs()`, a `void GpioSet()`, a class that is not a port |
| [`tests/mock_xmpl100.hpp`](../tests/mock_xmpl100.hpp) | The register model; the bus framing is in `mock_xmpl100_bus.hpp` |
| [`tests/check_version_copy.cmake`](../tests/check_version_copy.cmake) | `_config/Doxyfile` carries the CMake version |
| [`examples/host/`](../examples/host/README.md) | The host example, built with the test flags and run (`xmpl100_host_example`) |
| [`tests/check_install.cmake`](../tests/check_install.cmake) | Installs into a scratch prefix and builds [`tests/install_consumer/`](../tests/install_consumer/CMakeLists.txt) with `find_package(xmpl100)` |
| [`tests/check_doxygen.cmake`](../tests/check_doxygen.cmake) | Doxygen with `_config/Doxyfile` and warnings as errors, when `doxygen` is installed |

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
| Host tests | GCC and Clang build and run every test above, then build and run `examples/host` on its own |
| C++ lint, cppcheck | clang-format, clang-tidy and cppcheck over `inc/` and `src/` |
| Docs | Strict site build (`_config/mkdocs.yml`), figure and generated-region drift, Doxygen (`_config/Doxyfile`), offline link check, Lighthouse |
| Markdown lint, YAML lint | Repository text files |
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
