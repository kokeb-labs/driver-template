# AGENTS.md

<!-- scaffold:agents-intro -->
driver-template is the Kokeb Labs template for chip drivers and libraries, with a complete example driver for the
XMPL100, a fictional example sensor on SPI. It follows the
[Kokeb Labs engineering standard](https://github.com/kokeb-labs/.github/blob/main/standards/README.md)
(agents: `standards/agents.md`, drivers: `standards/driver.md`).
<!-- /scaffold:agents-intro -->

## Layout

- `inc/`, `src/`: the driver. `inc/xmpl100.hpp` is the umbrella header, `src/xmpl100.ipp` the implementation,
  `inc/xmpl100_spi_interface.hpp` the port, `inc/xmpl100_types.hpp` errors and config.
- `tests/`: host tests over the register model `tests/mock_xmpl100.hpp`, `tests/compile_fail/`, `tests/docs/`
  (every code block on the site), `tests/install_consumer/`.
- `docs/`: the site pages (`docs/.nav.yml` orders them). `docs/datasheets.yml` records the datasheet the driver
  follows (revision, SHA-256) and every topic it takes from it; `kokeb-docs gen` builds the README card, the
  Datasheet map (`docs/datasheet.md`) and each page's box from it (`tools/docs/generators/datasheet.py`).
- `examples/<platform>/`: the only platform code, one self-contained folder per platform with its own port.
  `examples/host/` (simulated chip, any PC) always exists and runs in the host tests.
<!-- scaffold:platform-esp32 -->
  `examples/esp32/`: the ESP-IDF example; the root `CMakeLists.txt` is the ESP-IDF component (`ESP_PLATFORM`).
<!-- /scaffold:platform-esp32 -->
- `_config/`: all tool configuration (clang-format, clang-tidy, `Doxyfile`, `mkdocs.yml`, `Kconfig`, linters).
  Pass the path (`--style=file:_config/.clang-format`, `--config-file=_config/.clang-tidy`,
  `mkdocs build -f _config/mkdocs.yml`); never add copies or symlinks at the root.
- `cmake/xmpl100_build_settings.cmake`: the only place the version is set; `driver.yml` repeats name and version,
  and CI checks they agree.
- `.kokeb/handbook.yml`: the handbook release this repository follows, its kind and assurance level, and any
  waivers (rule, reason, approver, expiry). CI checks the handbook rules against it and reports each finding by
  rule ID (`KB-CPP-003`, ...); fix the code, or add a dated waiver only with the maintainer's approval.
- The root holds only what GitHub or a tool needs there and what a reader looks for first: `README.md`, `LICENSE`,
  `CHANGELOG.md`, `CMakeLists.txt`, `AGENTS.md`, `CLAUDE.md`, `.gitignore`, `.gitmodules`, `driver.yml`.
<!-- scaffold:remove -->
- `tools/setup.py`, `tools/scaffold.py`, `tools/scaffold/`, `docs/template.md`,
  `.github/workflows/template-init.yml`: template only; see docs/template.md.
<!-- /scaffold:remove -->

## Layer rules

- A driver is MCU-agnostic and platform-free: no SDK, no RTOS, no lock, no heap, exceptions, RTTI, `std::function`,
  `std::string`, `printf` or static mutable state. It reaches the bus only through the port hooks.
- Everything is in `namespace xmpl100`; internals in `xmpl100::detail`. The class is `xmpl100::XMPL100`.
- The port base declares no hook; the driver is constrained by the `SpiPort` concept. Optional hooks are
  detected with `requires` in `xmpl100::detail` only.
- `Error` keeps the base set at values 0 to 9; chip-specific values start at 16 and each is returned somewhere.
- Every fallible call returns `Result<T>`; every port failure propagates. The constructor stores the configuration
  with no I/O; `EnsureInitialized()` is the only start-up call, and every I/O call returns `NotInitialized` before it
  has succeeded.
- A call sends only the traffic the datasheet needs. Any extra read-back or compare is a `Config` `bool`
  (`verify_writes`, `verify_reads`), off by default, with its cost in its comment (handbook KB-ARCH-041). Start-up
  checks in `EnsureInitialized()` and datasheet-required steps stay unconditional.
- Public and protected methods PascalCase, private methods camelBack, members `snake_case_`, constants
  `kPascalCase`, units as suffixes. 100 columns (`_config/.clang-format`).
- Kconfig values are read only in `inc/xmpl100_kconfig.hpp`.

## Build and test on the host

```bash
cmake -S . -B build -DXMPL100_BUILD_TESTS=ON
cmake --build build --parallel && ctest --test-dir build --output-on-failure
```

The tests build with `-fno-exceptions -fno-rtti -Werror`. CI runs GCC and Clang; run the compilers you have.
The host example runs as the CTest `xmpl100_host_example`. ESP32 builds run in CI only
(`esp32-examples-build-ci.yml`); do not install ESP-IDF to check a change.

## Docs checks

```bash
python3 -m venv .venv && .venv/bin/pip install \
  "git+https://github.com/kokeb-labs/ci-tools@v1#subdirectory=packages/kokeb-mkdocs-theme"
.venv/bin/kokeb-docs figures && .venv/bin/kokeb-docs gen    # after changing headers or snippets
.venv/bin/kokeb-docs check && .venv/bin/mkdocs build -f _config/mkdocs.yml --strict -d /tmp/site
```

Code shown on the site lives in `tests/docs/` between `// --8<-- [start:x]` / `[end:x]` markers. Doxygen runs as
the CTest `xmpl100_doxygen` when `doxygen` is installed; every public symbol in `inc/` is documented.

## Definition of done

The list in `standards/agents.md#definition-of-done`, in short: one change; standard followed or deviation
named; every check above passes locally (the `verify` skill); host tests for new behaviour including failure
paths; docs and Doxygen updated; `CHANGELOG.md` under `## [Unreleased]`; CI green including `CI gate / gate`
(which needs the handbook check, the Cortex-M build and the coverage, complexity and sanitizer gates);
anything hardware-dependent marked **not verified on hardware** with bench steps.

## Rules specific to this repo

- No attribution lines anywhere: no `Co-Authored-By:` trailers, no "Generated with" footers, no tool banners.
- Work on a `feat/`, `fix/`, `docs/`, `ci/` or `chore/` branch and open a PR; never push to `main`, force-push,
  merge, or change repository settings.
<!-- scaffold:remove -->
- In the template repository itself, a change to the XMPL100 shell must keep `tools/scaffold.py` working: the CI
  job `template-selftest` scaffolds a driver for every bus and a library and runs their checks.
<!-- /scaffold:remove -->

## Skills

`.claude/skills/`: `verify`, `release`, `bump-pin`, `add-driver-feature`, `new-driver-from-template`,
`datasheet-conformance`, `add-port`.
