---
name: add-driver-feature
description: Add one chip feature end to end - register model, API, tests, docs, changelog. Use when asked to support a register, mode or command the driver does not cover yet.
---

# Add a driver feature

One feature per PR, branch `feat/<feature>`. Cite the datasheet section for every register, bit and timing.

1. **Read the datasheet section** and write down: registers and bits, reset values, timing, error conditions, what
   the chip does on invalid input. Note the section numbers.
2. **Model it first.** Extend `tests/mock_xmpl100.hpp` so the register model behaves like the datasheet: reset
   values, read-only bits, self-clearing bits, timing (through `Elapse()`), and a fault-injection field for each
   failure the feature can hit.
3. **Write the failing tests** in `tests/test_xmpl100.cpp`: the register traffic of the feature, a call before
   `EnsureInitialized()` (`NotInitialized`), a failing port hook on every path (`Bus`), argument checks
   (`InvalidArgument`, nothing sent), and each new error. Run them: they fail.
4. **Add the registers** to `inc/xmpl100_registers.hpp` and the types (enums, `Config` fields with unit suffixes
   and datasheet defaults, a `Status` field) to `inc/xmpl100_types.hpp`; extend `Validate()`.
5. **Add the API** to `inc/xmpl100.hpp` (PascalCase, `noexcept`, `Result<T>`, Doxygen with units, return values,
   errors and whether it does I/O) and implement it in `src/xmpl100.ipp` (private helpers camelBack).
   A new chip-specific `Error` value takes the next free value from 16, gets a `ToString()` case and a scenario in
   `tests/test_errors.cpp`.
6. **Docs.** A compiled snippet in `tests/docs/api_tasks.cpp` and a section in `docs/api-guide.md`; update
   `docs/configuration.md`, `docs/contract.md` (errors table) and `docs/hardware.md` if they change. Run
   `kokeb-docs figures && kokeb-docs gen`.
7. **Changelog** under `## [Unreleased]` (`### Added`, or `### Breaking` with what callers change).
8. Run the `verify` skill and open the PR with the bench steps for the feature.
