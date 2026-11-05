---
name: datasheet-conformance
description: Check driver code against the chip's datasheet, citing sections, test first. Use when writing or reviewing register maps, timing, reset values or error handling.
---

# Datasheet conformance

<!-- scaffold:datasheet-link -->
The datasheet: the template's XMPL100 is fictional and has none.
<!-- /scaffold:datasheet-link -->

Every number in the driver has a source. Work one subsystem at a time (reset, identification, one mode, one
register block).

1. **Collect the facts** from the datasheet into a table in the PR description: register, address, bits,
   reset value, access (R, W, R/W, self-clearing, clear-on-read), timing (minimum, typical, maximum), and the
   section, table or figure number. Use the maximum for waits and the minimum for pulses.
2. **Write the model first.** Make `tests/mock_<chip>.hpp` follow the table: reset values, read-only and
   self-clearing bits, clear-on-read flags, timing, invalid writes.
3. **Write the tests** that pin each fact: reset values read back after `Initialize()`, the exact bytes of each
   frame, the waits (sum of `DelayUs()` against the datasheet maximum), every documented error condition.
4. **Then change the driver** until the tests pass. Each register access and wait carries a comment with the
   section (`// t_RST, section 8.2`).
5. **Cross-check** `Config{}` against the reset values, `Validate()` against the documented ranges, and every
   `Error` against a datasheet condition or a port failure.
6. **Mark what you could not verify**: values the datasheet leaves open, errata, anything inferred. Say so in the
   code comment and the PR, and list it under "not verified on hardware" with a bench step.

Never invent a register, bit or timing. If the datasheet is silent, ask, or leave the feature out.
