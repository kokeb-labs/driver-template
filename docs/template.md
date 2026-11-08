# Using driver-template

This repository is the starting point for every chip driver and standalone library of Kokeb Labs. It holds a
complete, working driver for the **XMPL100**, a fictional example SPI temperature sensor, inside the full repository
shell (build, tests, docs, CI, governance, agent skills), and a setup tool that turns it into your driver or library.
Everything follows the [Kokeb Labs engineering standard](https://github.com/kokeb-labs/.github/tree/main/standards);
this page says how the template maps onto it. The setup deletes this page (`docs/template.md`).

## Create a repository from it

```bash
gh repo create kokeb-labs/driver-<chip> --public --template kokeb-labs/driver-template --clone
cd driver-<chip> && git switch -c feat/initial-driver
python3 tools/setup.py
```

`tools/setup.py` reads the repository name from `git remote get-url origin`: `driver-<chip>` is a driver for
`<chip>`, any other name a library (`ring-buffer` is the library `ring_buffer`). In a terminal it asks only for what
it cannot infer: the bus and a one-sentence description, and the C++ namespace for a library. It then runs
`tools/scaffold.py` in place, configures, builds and runs the host tests, and prints a short checklist. Commit the
result signed off (`git commit -s`): the DCO check needs a `Signed-off-by` line on every commit.

Without a terminal (CI, a script, an agent) every answer is a flag:

```bash
python3 tools/setup.py --non-interactive --bus i2c --description "C++20 driver for the TMP117 temperature sensor."
python3 tools/setup.py --non-interactive --kind library --name ring_buffer --namespace ring \
  --description "Fixed-capacity ring buffer, no heap."
```

`--datasheet <url>` writes the datasheet link into `docs/datasheets.yml` (`url`), the Hardware page and the
`datasheet-conformance` skill. Only the link: the setup never downloads the PDF, and no vendor PDF is committed.
Everything else in `docs/datasheets.yml` (vendor, number, revision, date, `pdf`, `sha256` and the topics) stays the
fictional XMPL100 placeholder, which `kokeb-docs gen` accepts, until you fill it in; the checklist says so.

The first push to a new repository runs `.github/workflows/template-init.yml`, which opens one issue, "Finish
setup", with the exact `tools/setup.py` command for that repository. It cannot run the setup itself, because the bus
cannot be inferred from the name. The workflow only has `issues: write`, skips the template itself, and the setup
deletes it.

Then follow the checklist, the `new-driver-from-template` skill, and [Repository setup](repo-setup.md) for the org
settings an owner applies.

## Setup and scaffold options

`tools/setup.py` passes these to `tools/scaffold.py` and adds `--non-interactive` (ask nothing) and `--no-build`
(skip configure, build and test). `tools/scaffold.py` needs `--kind` and `--name`; the setup infers them.

| Option | Default | Meaning |
| --- | --- | --- |
| `--kind driver\|library` | from the repository name | A chip driver on a port, or a platform-free library |
| `--name` | from the repository name | Lower case, digits, underscores: the part number (`tmp117`) or the library name (`ring_buffer`) |
| `--bus spi\|i2c\|uart\|adc\|none` | `spi` (driver), `none` (library) | The port the driver gets; `none` is a driver for a part with GPIO pins only |
| `--namespace` | the name | The C++ namespace; nested names allowed (`acme::tmp117`). The CMake package and target stay `<name>` and `<name>::<name>` |
| `--description` | a generic sentence | One sentence for the README, the docs and CMake |
| `--platforms` | `host,esp32` (driver), `host` (library) | The example platforms: `host` (always included) and `esp32`. `esp32` adds `examples/esp32/`, its CI job, the ESP-IDF branch of the root `CMakeLists.txt` and `_config/Kconfig`. An unknown platform is an error that points at the `add-port` skill |
| `--include-root` | the name | Library only: headers live in `inc/<root>/` and are included as `"<root>/X.h"` |
| `--org` | the owner in `git remote get-url origin`, else `kokeb-labs` | The GitHub organisation in repository, docs-site and discussion links |
| `--repo` | the remote's repository name, else `driver-<name>` / `<name>` | The repository name in links |
| `--author` | `The <Org> Authors` (`The Kokeb Labs Authors` for `kokeb-labs`) | The copyright holder in `LICENSE` and the source headers. Organisations with a display name are listed in `ORGS` at the top of `tools/scaffold.py` |
| `--license MIT\|Apache-2.0` | `MIT` | `LICENSE` gets the full text (`tools/scaffold/license/`); the SPDX identifiers, the README badge and the licence line follow |
| `--datasheet` | none | Driver only: the datasheet URL, written into `docs/datasheets.yml` and linked on the Hardware page and in the `datasheet-conformance` skill; never fetched |
| `--out DIR` | in place | `scaffold.py` only: write a scaffolded copy instead of rewriting this checkout |
| `--selftest` | | `scaffold.py` only: scaffold every case into temporary directories and check each (what CI runs) |
| `--check-drift DIR` | | `scaffold.py` only: compare the files the template manages in the repository at `DIR` with a fresh scaffold ([Template drift](#template-drift)); `--fix` takes the template's version |

## What the scaffold changes

1. **Names.** `XMPL100` becomes the upper-case name, `Xmpl100` the PascalCase name, `xmpl100` the name, in every
   file and path; `namespace xmpl100` and `xmpl100::` become `--namespace`. The result carries no organisation
   prefix in code: the namespace is `<chip>::`, the CMake package `find_package(<chip>)`, the target
   `<chip>::<chip>` (`EXPORT_NAME <chip>`), the files `cmake/<chip>_build_settings.cmake` and
   `cmake/<chip>Config.cmake.in`, the macros `<CHIP>_*`, the Kconfig options `CONFIG_<CHIP>_*` and the version
   header `<chip>_version.h`.
2. **Organisation.** Links to the repository, its docs site and the organisation's discussions take `--org` and
   `--repo`; the copyright holder takes `--author`; `LICENSE` and the SPDX identifiers take `--license`. Links to the
   standard (`kokeb-labs/.github`) and the shared workflows (`kokeb-labs/ci-tools/.github/workflows/<name>.yml@v1`)
   stay, and appear only in CI, governance and documentation files, never in code; `--selftest` checks that.
3. **Bus.** For `i2c`, `uart`, `adc` or `none` it swaps in the port header (`inc/<name>_<bus>_interface.hpp`, with
   the standard hooks of that bus; `inc/<name>_gpio_interface.hpp` for `none`), the mock transport, the ports on the
   docs pages (`tests/docs/my_port.hpp`, `ports.cpp`), the install-consumer port and the ESP-IDF port; `SpiInterface`
   and `SpiPort` become `<Bus>Interface` and `<Bus>Port` (`GpioInterface`, `GpioPort`). Bus-specific prose sits
   between `<!-- scaffold:NAME -->` markers and is replaced from `tools/scaffold/bus/<bus>/blocks/`. `adc` and
   `none` also replace the register-based core: `adc` with an analog one (a range check instead of the chip ID),
   `none` with a GPIO-only one (a temperature switch read on one pin, with a self-test pulse instead of the chip
   ID). Neither has a register map.
4. **Library.** It drops the chip: the port, the register map and mock, the chip-ID logic, the ESP32 port, and the
   driver-only pages (hardware, porting, configuration, troubleshooting, architecture) and skills, and the
   datasheet record: `docs/datasheets.yml`, the Datasheet map, its generator and every datasheet region (the
   library README has no Datasheet section). It adds
   `inc/<root>/` with a `MovingAverage` example utility, `Result.h`, `Kconfig.h` and a CMake-configured `Version.h`,
   host tests in `tests/host/`, the library page set (concepts, integration, platforms), a host example and, with
   `--platforms host,esp32`, a no-hardware ESP32 example.
5. **Platforms.** `examples/host/` always stays. Without `esp32` in `--platforms` the scaffold deletes
   `examples/esp32/`, `.gitmodules`, the ESP32 CI workflow and its job in the gate, the ESP-IDF branch of the
   root `CMakeLists.txt` and every `scaffold:platform-esp32` block of the docs; `_config/Kconfig` and the Kconfig
   header stay, so adding ESP-IDF later is the CMake branch and the example. A library defaults to `host`: it
   gets ESP-IDF only when it is meant to be consumed there.
6. **Template-only parts** go: this page, `tools/setup.py`, `tools/scaffold.py`, `tools/scaffold/`, the
   `template-selftest` and `template-init` workflows and the self-test line in the CI gate, the "fictional part"
   notes. The docs workflow publishes (`deploy: true`). Every `scaffold:` marker is removed.
7. **Changelog** restarts with one `## [Unreleased]` entry. The version stays `0.1.0`.
8. **Formatting and docs.** With `clang-format` installed it formats the C++ sources (the new names change the
   include order); with `kokeb-docs` installed it redraws the figure and refreshes the generated regions (the README
   quickstart, the Examples table). Otherwise the checklist says to.
9. **Record.** `.kokeb/template.yml` keeps the template commit and the arguments, for the drift check.

## The repository root

The root holds only what GitHub or a tool requires there and what a reader looks for first
([ADR-0011](https://github.com/kokeb-labs/.github/blob/main/standards/adr/0011-repository-root-layout.md)):

- Files: `README.md`, `LICENSE`, `CHANGELOG.md`, `CMakeLists.txt`, `AGENTS.md`, `CLAUDE.md`, `.gitignore`,
  `.gitmodules`.
- Folders: `.github/` (workflows, `CONTRIBUTING.md`, `SECURITY.md`), `.claude/`, `_config/`, `cmake/`, `docs/`,
  `examples/`, `inc/`, `src/`, `tests/`, `tools/`.

All tool configuration is in `_config/`, and every command names it (`--style=file:_config/.clang-format`,
`--config-file=_config/.clang-tidy`, `mkdocs build -f _config/mkdocs.yml`, `doxygen _config/Doxyfile` from the
root); there are no copies or symlinks at the root. The setup tools live in `tools/`, not at the root. The one
`CMakeLists.txt` is both the CMake library and, under `ESP_PLATFORM`, the ESP-IDF component, so no platform needs a
manifest or a wrapper folder. `--selftest` fails when a scaffolded root holds anything else, or a symlink.

## What each file is for

| Path | What it is | Standard |
| --- | --- | --- |
| `inc/xmpl100.hpp` | Umbrella header: the driver class `xmpl100::XMPL100<Port>`, constrained by the port concept; optional hooks detected in `xmpl100::detail` | driver.md: Namespace, Lifecycle |
| `inc/xmpl100_spi_interface.hpp` | The CRTP base `SpiInterface<Derived>` (declares no hook), `CtrlPin`, `GpioSignal`, the `SpiPort` concept, the register framing | driver.md: The port |
| `inc/xmpl100_types.hpp` | `Error` (base set 0 to 9, `UnexpectedReset` = 16), `Result<T>`, `ToString()`, `Config` with datasheet defaults and unit suffixes, presets, `Validate()`, version getters | driver.md: Errors, Config |
| `inc/xmpl100_registers.hpp` | The register map with section references; the CRC-8 | driver.md: layout |
| `inc/xmpl100_kconfig.hpp`, `_config/Kconfig` | The only place `CONFIG_XMPL100_*` is read: `KconfigDefaults()` | driver.md: Config and Kconfig |
| `inc/xmpl100_version.h.in` | Version header template, configured into the build tree | driver.md: Build |
| `src/xmpl100.ipp` | The implementation, included at the end of the umbrella header; private helpers camelBack | naming-and-style.md |
| `cmake/xmpl100_build_settings.cmake` | The single source of the version, target name, sources and IDF requirements | driver.md: Build |
| `cmake/xmpl100Config.cmake.in`, `CMakeLists.txt` | INTERFACE target `xmpl100`, the `xmpl100::xmpl100` alias, `EXPORT_NAME`, install rules, package config; under `ESP_PLATFORM` the ESP-IDF component (`idf_component_register`, `KCONFIG _config/Kconfig`). No `idf_component.yml`: only registry publishing needs one | driver.md: Build |
| `examples/host/` | The host example: a host port over the register model and a program that initialises and reads; built and run by the host tests and on its own in CI | driver.md: Examples |
| `examples/esp32/` | ESP-IDF project: `app_config.yml`, `sdkconfig.defaults` (`CONFIG_FREERTOS_HZ=1000`), `EXTRA_COMPONENT_DIRS` at the repository root, `main/` with the port and a basic example, `scripts/` (submodule) | driver.md: Examples |
| `tests/mock_xmpl100.hpp`, `tests/mock_xmpl100_bus.hpp` | Register model of the chip; mock ports with and without the optional hooks | driver.md: Tests |
| `tests/test_xmpl100.cpp` | Lifecycle, configuration, measurement, every failure path | driver.md: Tests |
| `tests/test_errors.cpp` | Base `Error` values fixed; every chip-specific value returned by some call | driver.md: Errors |
| `tests/test_version.cpp`, `tests/check_version_copy.cmake` | Version getters follow CMake (also against a 9.8.7 probe); `_config/Doxyfile` agrees | driver.md: Tests |
| `tests/test_kconfig.cpp` | `KconfigDefaults()` with and without `CONFIG_` values | |
| `tests/compile_fail/` | Code that must not compile, each with its expected diagnostic (`EXPECT:`) | driver.md: Optional hooks |
| `tests/docs/` | Every C++ block on the site, compiled | docs.md: Code on the site |
| `tests/install_consumer/`, `tests/check_install.cmake` | `cmake --install`, then `find_package(xmpl100)` and `xmpl100::xmpl100` | driver.md: Tests |
| `tests/check_doxygen.cmake`, `_config/Doxyfile` | Doxygen with `EXTRACT_ALL = NO`, `WARN_AS_ERROR = FAIL_ON_WARNINGS` on `inc/` | docs.md: Doxygen |
| `docs/`, `_config/mkdocs.yml`, `docs/.nav.yml` | The driver page set on the shared theme (`kokeb-mkdocs-theme`), with `contract.md` under Concepts, `platforms.md` and `repo-setup.md`; this page is template only | docs.md |
| `docs/diagrams/` | The layer figure (figkit), drawn from the headers | docs.md |
| `tools/docs/generators/snippets.py` | The README's quickstart region, copied from the compiled snippet | docs.md: README |
| `docs/datasheets.yml`, `docs/datasheet.md`, `tools/docs/generators/datasheet.py` | The datasheet record (document, revision, SHA-256, and each topic with its section, page, driver API and docs pages); the generator builds the README and home-page card (`datasheet-card`), the Datasheet map (`datasheet-map`) and the box after each page's intro (`datasheet-refs`). The template's is a fictional placeholder; drivers only | ADR-0019 |
| `.github/workflows/ci.yml` | Runs every check, DCO (pull requests only) and the org-standard check, and ends in `CI gate` (`CI gate / gate`) | repo-governance.md: The required check |
| `.github/workflows/ci-*.yml`, `esp32-examples-build-ci.yml` | One check each, calling the shared workflows in `kokeb-labs/ci-tools` at `@v1`; callable alone | |
| `.github/workflows/release.yml`, `.github/dependabot.yml` | Release on a `v*` tag; Actions updates | release.md, repo-governance.md |
| `.github/workflows/template-selftest.yml` | Template only: scaffolds every case and runs its checks | |
| `.github/workflows/template-init.yml` | Template only: in a new repository, opens the "Finish setup" issue with the setup command | |
| `.github/CODEOWNERS`, `pull_request_template.md`, `ISSUE_TEMPLATE/`, `labels.yml` | Governance; the PR template has a bench-status section | repo-governance.md |
| `.github/CONTRIBUTING.md`, `.github/SECURITY.md`, `LICENSE`, `CHANGELOG.md` | Branch names, squash merges, definition of done; private reports; MIT (or Apache-2.0 with `--license`); Keep a Changelog with `### Breaking` | repo-governance.md, release.md |
| `_config/` | All tool configuration: the org `.clang-format` and `.clang-tidy` (copied unchanged from the standard), `Doxyfile`, `mkdocs.yml`, `Kconfig`, markdownlint, yamllint, lychee. No copies or symlinks at the root ([Editor setup](porting.md#editor-setup)) | naming-and-style.md, ADR-0011 |
| `AGENTS.md`, `CLAUDE.md`, `.claude/skills/` | Agent entry point and skills: `verify`, `release`, `bump-pin`, `add-driver-feature`, `new-driver-from-template`, `datasheet-conformance`, `add-port` | agents.md |
| `tools/setup.py` | Template only: the one entry point; infers, asks, scaffolds, builds and tests | |
| `tools/scaffold.py`, `tools/scaffold/` | Template only: the scaffold, its organisation settings (`ORGS`, `LICENSES`), the overlays (`bus/<bus>/`, `library/`: files ending in `.tmpl`, and `blocks/` for marked prose) and the licence texts (`license/`) | |

## Changing the template

Keep the XMPL100 working as a real driver: every check of the `verify` skill passes on the template itself. A
change to a shared file (CMake, tests, docs, workflows) must also work for every case the scaffold produces: run
`python3 tools/scaffold.py --selftest` (CI runs it as the `template-selftest` job). The self-test covers a driver on
every bus (`none` included), a host-only driver, a driver for another organisation under Apache-2.0 with a datasheet
link, and two libraries; it fails on a template name, a foreign organisation name or an organisation link left in
code. Overlay files mirror the template paths under `tools/scaffold/<kind>/files/` with a `.tmpl` suffix, so linters
and the build skip them.

## Template drift

A scaffolded repository keeps its own copy of the template's shell, so a fix here does not reach it by itself. The
drift check finds the copies that fell behind. `python3 tools/scaffold.py --check-drift DIR` reads
`DIR/.kokeb/template.yml`, scaffolds this checkout again with those arguments into a temporary directory and compares
every file the template manages:

- managed: `.github/`, `_config/` (except `Kconfig`), `.claude/skills/`, `cmake/` (except
  `<name>_build_settings.cmake`), `CLAUDE.md` and the `tests/check_*.cmake` helpers;
- the repository's own: code, tests, docs, the README, the changelog, `driver.yml`, `_config/Kconfig`.

Each scaffolded repository runs it in CI as the `Template drift` job (ci-tools `ru-template-drift`), which warns on
each drifted file. `--fix` replaces the drifted files with the template's version and records the new template
commit; review that diff, because a difference can be a fix the repository made first that belongs here. A file that
differs on purpose goes under `local` in the record, with the reason. A change to a managed file here therefore
shows up as a warning in every repository made from the template until each takes it.

For a local docs check, install the theme in a venv:
`pip install "git+https://github.com/kokeb-labs/ci-tools@v1#subdirectory=packages/kokeb-mkdocs-theme"`, which
provides `kokeb-docs` and `mkdocs`.
