---
name: verify
description: Run locally every check CI runs (host tests, doc snippets, format, tidy, markdownlint, docs). Use before every push and before opening a PR.
---

# Verify

Run from the repository root. Tool configuration is in `_config/`: pass its path, never copy it to the root.
Stop at the first failure, fix the cause, start again. Never weaken a check to pass.

1. Configure and build the host tests, doc snippets and compile-fail tests:
   `cmake -S . -B build -DXMPL100_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug && cmake --build build --parallel`
2. Run them: `ctest --test-dir build --output-on-failure`. This includes the version-copy tests, the install
   consumer, the host example (`examples/host`) and, when `doxygen` is installed, Doxygen with warnings as errors.
3. If both GCC and Clang are installed, repeat 1 and 2 with `-DCMAKE_CXX_COMPILER=g++` and `clang++` in separate
   build directories.
4. Format the changed C++ files: `clang-format --style=file:_config/.clang-format -i <files>`, then
   `git diff --exit-code` shows nothing new.
5. Tidy the changed files in `inc/` and `src/` when clang-tidy is installed:
   `cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` then
   `clang-tidy -p build --config-file=_config/.clang-tidy <files>` on the changed `.cpp` files and the tests
   that include the changed headers; fix findings in changed lines.
6. Markdown:
   `npx markdownlint-cli2 --config _config/.markdownlint.json "**/*.md" "#build*" "#examples/esp32/scripts"`
   (skip if Node is not available and say so in the PR).
7. Docs (in a venv with the docs theme, see AGENTS.md):
   `kokeb-docs figures && kokeb-docs gen`, then
   `kokeb-docs check && mkdocs build -f _config/mkdocs.yml --strict -d /tmp/site`.
   Commit any figure or generated region that changed. CI builds the docs without submodules; if
   `examples/esp32/scripts` is checked out, run `git submodule deinit -f examples/esp32/scripts` first (its
   README has figure markers of its own), and `git submodule update --init` afterwards.
8. In the template repository only: `python3 tools/scaffold.py --selftest` scaffolds every kind into temporary
   directories, checks that each scaffolded root holds only the allowed files and no template or foreign names, and
   runs steps 1, 2 and 7 on each.
9. Delete the build directories (`build*/`, `/tmp/site`) when done.

Report what ran and what could not run locally (ESP32 builds always run in CI only).
