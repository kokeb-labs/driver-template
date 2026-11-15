# Language standard

The driver declares the C++ standard it needs once, in the `language:` block of `driver.yml`. Everything else that
names the standard (the CMake files, the compiler flags, `.clang-format`, the workflows, the README badge and the
sentences that say "a C++20 compiler") repeats that declaration, and `tools/check_language.py` keeps them equal.

```yaml
language:
  standard: 20               # the minimum ISO C++ standard: a consumer needs at least this
  profile: KB-CPP20          # the handbook profile of that standard
  profile_version: "1.0"     # the handbook release that defines it
  misra: "MISRA C++:2023"    # the MISRA edition the driver is analysed against
  compilers:                 # the oldest versions the driver promises to build with (quote them)
    gcc: "10"
    clang: "13"
  tested: [20]               # every standard CI compiles the headers under; includes the minimum
  effective_from: 2026-11-15 # the date this declaration took effect
```

## What is checked

`python3 -I tools/check_language.py` (part of the preflight and the repository checks) fails when

- the block is missing a key, has an unknown one, or names a profile that is not `KB-CPP<standard>`;
- `tested` does not include `standard`, or names a standard below it;
- `cxx_std_N` or `CMAKE_CXX_STANDARD` in a CMake file, `Standard:` in `.clang-format`, or a `__cplusplus` assertion
  names another standard, or the library target never requires the declared one;
- a `-std=c++N` flag or a workflow `std` input names a standard that is not in `tested`;
- the README badge, "a C++20 compiler", or "GCC 10+" and "Clang 13+" differ from the block.

A `driver.yml` without the block is a note, so a driver that has not taken the template yet still passes;
`python3 tools/scaffold.py --check-drift --fix` adds the block. Whether the profile is known, current, supported or
retired is not decided here but by the organisation's registry, which kokeb-labs/ci-tools keeps
(`tools/standards/standards.yml`, checked by `tools/standards/check_standards.py`).

## Raising the standard

Raising `standard` is a breaking change for everyone who builds with an older compiler (a minor release before 1.0, a
major release after). Change the block, run `python3 -I tools/check_language.py --fix` to rewrite every site, read the
diff (prose about the profile's own rules is left alone on purpose), then run the preflight. The full playbook (profile
differences, feature gates, a multi-standard CI matrix, the review items, the MISRA edition) is kept with the registry
in kokeb-labs/ci-tools, `docs/language-standards.md`.
