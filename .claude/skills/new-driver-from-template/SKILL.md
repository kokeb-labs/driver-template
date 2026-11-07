---
name: new-driver-from-template
description: Turn a repository created from driver-template into a real driver or library - run tools/setup.py, then fill in the register map, port, tests and docs. Use right after `gh repo create --template kokeb-labs/driver-template`.
---

# New driver from the template

The template's example chip is the fictional XMPL100. `docs/template.md` explains every file.

## 1. Scaffold

If `tools/setup.py` is gone, the setup already ran: start at step 2.

1. Branch `feat/initial-driver`.
2. Run `python3 tools/setup.py` in place. It infers the kind and name from the repository name (`driver-<chip>`
   is a driver, any other name a library), asks for the bus (`spi`, `i2c`, `uart`, `adc`, or `none` for a part
   with GPIO pins only) and a one-sentence description (and the namespace for a library), scaffolds, then builds
   and runs the host tests. Without a terminal pass the answers as flags:
   `python3 tools/setup.py --non-interactive --bus i2c --description "<one sentence>"`. Add
   `--datasheet <url>` to link the datasheet (only the link: never download or commit the PDF). `--org`,
   `--repo`, `--author` and `--license MIT|Apache-2.0` override what it reads from the git remote;
   `--platforms` picks the example platforms (`host,esp32` for a driver, `host` for a library).
3. Read the checklist it prints; it is also the PR description's to-do list.
4. Run the `verify` skill: the renamed example must still build, pass and publish before you change anything.
   Commit this state on its own, signed off (`git commit -s -m "Scaffold <CHIP> from driver-template"`).

## 2. Replace the example chip (drivers)

Use the `datasheet-conformance` skill for every step, one subsystem at a time:

1. `inc/<chip>_registers.hpp`: the real register map with datasheet section references.
2. `inc/<chip>_types.hpp`: the chip ID, timing constants, enums, `Config` with the real reset values,
   presets, `Validate()`. Keep the base `Error` set; add chip-specific values from 16 only when a call returns them.
3. `tests/mock_<chip>.hpp`: the register model of the real chip, then the tests, then the driver
   (`inc/<chip>.hpp`, `src/<chip>.ipp`).
4. The port (`inc/<chip>_<bus>_interface.hpp`): keep the standard hooks for the bus; add a hook only for a
   protocol that does not fit, and list it on the Porting page.
5. `examples/host/`: the host port over the new register model and the example program.
6. `examples/esp32/main/` (when scaffolded with `esp32`): the ESP-IDF port pins and the example.

## 3. Docs and metadata

1. `docs/datasheets.yml`: it still holds the fictional XMPL100 placeholder (only `url` is set, by `--datasheet`).
   Fill in the real datasheet's vendor, number, revision, date, `url`, `pdf` (a stable direct link or `null`) and
   the `sha256` of the PDF you used (never commit the PDF), and one topic per section the driver uses (section,
   printed page, driver API, the docs pages that explain it). Every docs page a topic names keeps its
   `<!-- generated: datasheet-refs -->` box; remove the box from a page no topic names. A library has none of this.
2. Every page in `docs/`: the chip card, pins, timing, power, errors; the README's paragraph, At a glance table,
   wiring and documentation table. Remove every statement about the XMPL100.
3. `kokeb-docs figures && kokeb-docs gen`, `mkdocs build -f _config/mkdocs.yml --strict`.
4. `CHANGELOG.md`: `## [Unreleased]` / `### Added` describing the first version.
5. Run `verify`, open the PR, and give the owner `docs/repo-setup.md` for the org settings.
