# Porting

Run the driver on a new MCU by writing one class: the port.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Example Corp (fictional) XMPL100-DS A:

    - SPI mode, clock and register framing: 6.2 SPI interface, p. 8
    - Soft reset and t_RST: 8.2 Soft reset, p. 17
    - Hardware reset (RESET_N) and t_RST_PW: 8.3 Hardware reset, p. 18

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

The driver never calls your SDK. It calls hooks on a class you derive from `xmpl100::SpiInterface<Self>` (in
[`inc/xmpl100_spi_interface.hpp`](../inc/xmpl100_spi_interface.hpp)). The base uses CRTP and declares no hook
itself; the driver template is constrained by the `xmpl100::SpiPort` concept, so calls are resolved at compile
time (no virtual functions, nothing to allocate) and a missing or misdeclared hook is a compile error at the point
of use.

<!-- scaffold:bus-port -->
## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the bus up on the first call; return `true` at once afterwards. `false` makes the driver's `EnsureInitialized()` return `Bus` |
| `bool Transfer(const uint8_t* tx, uint8_t* rx, std::size_t len) noexcept` | yes | One full-duplex transfer of `len` bytes with CS_N low for all of them. `rx` is `nullptr` for writes. `false` on any bus error |
| `void DelayUs(uint32_t us) noexcept` | yes, for every wait | Wait at least `us` microseconds |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | no | Drive RESET_N; `false` if the pin write failed |
| `void Log(LogLevel, const char* message) noexcept` | no | Print one line; `message` is plain text, never a format string |

## The port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

Rules the transfer must follow:

- **One call, one frame.** Keep CS_N low for all `len` bytes; the chip decodes the frame when CS_N rises. Never
  split a burst into several transfers: the address auto-increment does not carry over.
- **The bus requirements below**: mode 0 or 3, MSB first, up to `kMaxClockHz`. Start at 1 MHz on jumper wires.
- **Raw bytes only.** Do not interpret, retry or reorder bytes; the driver owns the framing and the retries.
- **Return `false` for a bus error.** The driver turns it into `Error::Bus` and never ignores it.
- **Bounded time-outs.** A stuck bus fails the call; it does not block forever.

## Bus requirements

The driver states what the XMPL100 needs from the bus as `constexpr` values in
[`inc/xmpl100_spi_interface.hpp`](../inc/xmpl100_spi_interface.hpp) (handbook KB-CPP-123). Check your port's
settings against them with `static_assert`, as both example ports do, so a port that clocks too fast or in the
wrong mode does not compile:

| Constant | Value | Meaning (datasheet 6.2) |
| --- | --- | --- |
| `kSpiMode` | `0` | CPOL 0, CPHA 0; mode 3 also works |
| `kMsbFirst` | `true` | Most significant bit first |
| `kMaxClockHz` | `10'000'000` | f_SCK max |
| `kMinCsHighNs` | `50` | CS_N high between two transfers, t_CSH |
| `kCsSetupNs`, `kCsHoldNs` | `20`, `20` | CS_N low to the first SCK edge, last SCK edge to CS_N high |
| `kContinuousWord` | `false` | Idle SCK time between the bytes of one transfer is allowed |

<!-- /scaffold:bus-port -->

## Control pins

Each control pin has a defined idle level, and the port drives it there before it enables the pin's output and
before the driver's first transfer (handbook KB-HW-018). Configuring a pin as an output first and setting its level
afterwards gives the chip a pulse it did not ask for, for example a reset.

| `CtrlPin` | Package pin | Polarity | Idle (`INACTIVE`) level | Before the first transfer |
| --- | --- | --- | --- | --- |
| `Reset` | RESET_N | active low | high | Set high, then enable the output |

`GpioSignal::ACTIVE` always means "function asserted"; the port maps it to the electrical level. A pin tied on the
board (RESET_N tied high) needs no hook.

## Optional hooks

The driver finds the optional hooks at compile time with C++20 `requires`, in one place
(`xmpl100::detail`). A missing hook falls back in a documented, safe way or fails to compile; it never skips a
step silently.

| Hook | Used for | Without it |
| --- | --- | --- |
| `DelayUs()` | t_RST after a reset; 1 ms between polls while a one-shot converts | Compile error at the first call that waits (every port needs it in practice) |
| `GpioSet()` | RESET_N: the reset in `EnsureInitialized()` and `HardwareReset()` | `EnsureInitialized()` soft-resets; `HardwareReset()` returns `Error::Unsupported` |
| `Log()` | One line per lifecycle event or failure | Nothing is logged |

A port with every hook:

```cpp
--8<-- "tests/docs/ports.cpp:full-port"
```

```cpp
--8<-- "tests/docs/ports.cpp:hardware-reset"
```

A `GpioSet()` that returns `void` is a compile error: the driver must see a failed pin write. The
[compile-fail tests](testing.md) check both rules.

## Reference implementations

Every port lives with its example, in `examples/<platform>/`; the driver has none. [Platforms](platforms.md) lists
them and how to add one.

- [`examples/host/host_xmpl100_port.hpp`](../examples/host/host_xmpl100_port.hpp): any PC, against a simulated
  chip. Start from it for a host build or a test bench; it is also the smallest complete port.
<!-- scaffold:platform-esp32 -->
- [`examples/esp32/main/esp32_xmpl100_port.hpp`](../examples/esp32/main/esp32_xmpl100_port.hpp): ESP-IDF, with
  every hook. Start from it for any ESP-IDF target.
<!-- /scaffold:platform-esp32 -->

## Check your port

1. Build it: the ports on this page are compiled by the host CI from [`tests/docs/`](../tests/docs/my_port.hpp),
   with the driver instantiated over them.
2. Call `EnsureInitialized()`: `Ok` proves that writes and reads work and that CHIP_ID read `0xA1`.
3. Call `ReadTemperatureC()` twice a second and warm the chip with a finger: the value rises within seconds.

## Threads

The driver is not thread-safe. Give each driver object one owner task, or guard every call with one mutex. When
several chips share a bus, the port must also serialise the bus. See [Contract](contract.md#threading).

## Editor setup

The clang-format and clang-tidy configuration is in `_config/`, and the repository root has no copy of it. Tools
that take a path use it directly; CI does the same:

| Tool | Setting |
| --- | --- |
| Command line | `clang-format --style=file:_config/.clang-format`, `clang-tidy --config-file=_config/.clang-tidy -p build` |
| VS Code (C/C++ extension) | `"C_Cpp.clang_format_style": "file:${workspaceFolder}/_config/.clang-format"` and `"C_Cpp.codeAnalysis.clangTidy.args": ["--config-file=${workspaceFolder}/_config/.clang-tidy"]` |
| clangd (VS Code clangd, Neovim, Emacs), CLion | These find `.clang-format` and `.clang-tidy` only by name, from the file upwards: copy both to the root once (`cp _config/.clang-format _config/.clang-tidy .`). `.gitignore` lists the copies, so they never reach a commit; copy again after `_config/` changes |

Configure with `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` (clangd: `--compile-commands-dir=build`) so the editor sees the
same flags as the build.
