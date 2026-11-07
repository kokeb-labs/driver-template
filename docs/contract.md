# Contract

What the XMPL100 driver promises, what it needs, and the rules a caller follows.

The driver inherits the [Kokeb Labs driver contract](https://github.com/kokeb-labs/.github/blob/main/standards/driver.md);
this page states only what is specific to it. A change to anything here is a breaking change, listed under
**Breaking** in the [changelog](changelog.md).

## Promises

- The constructor is `noexcept`, stores the configuration, does no I/O and cannot fail.
- `EnsureInitialized()` is the only way to start the driver. Its first call checks the stored configuration
  (`InvalidArgument`, nothing sent), brings the bus up, resets the chip (RESET_N when the port has `GpioSet()`,
  otherwise CTRL.SOFT_RESET), checks CHIP_ID and only then writes the configuration. On any failure the driver
  stays uninitialised and the next call starts again from the beginning; once it has succeeded, later calls return
  `Ok` with no I/O.
- Every call that touches the chip returns `Error::NotInitialized` before `EnsureInitialized()` succeeds. No call
  initialises or resets the chip as a side effect. `ApplyConfig()` before the start only stores the configuration.
- Every port failure propagates as `Error::Bus`; no getter returns a stale or zero value after a failed read.
- Every wait is bounded: a reset by `kResetTimeMaxUs` plus three STATUS reads, a one-shot by
  `Config::conversion_timeout_ms`.
- `Config{}` equals the datasheet reset values.
- A missing `DelayUs()` is a compile error; a missing `GpioSet()` gives `Error::Unsupported` from
  `HardwareReset()`; a missing `Log()` logs nothing. A misdeclared hook is a compile error.
- `Deinitialize()` leaves the chip in shutdown, its lowest-current state.

## Depends on

| Dependency | Version | Why |
| --- | --- | --- |
| A C++20 compiler | GCC 10, Clang 13 or later | Concepts constrain the port |
| CMake | 3.16 or later | Generates `xmpl100_version.h` |
| Your port | | The bus, the delay and optionally RESET_N and logging ([Porting](porting.md)) |

Nothing else: no RTOS, no SDK, no other repository of the org.

## Threading

The driver is not thread-safe and takes no lock. Use each driver object from one task at a time, or guard every
call on it with one mutex; the layer that owns the driver owns that lock, and the bus lock when several chips
share a bus. Nothing may run in an ISR: every call can block for a bus transfer or a delay. Separate objects
share no state.

## Errors

`xmpl100::Error` inside `xmpl100::Result<T>`. Values 0 to 9 are the shared base set; this driver returns every
one of them, plus one chip-specific value.

| `Error` | Value | Returned when |
| --- | --- | --- |
| `Ok` | 0 | The call succeeded |
| `NotInitialized` | 1 | `EnsureInitialized()` has not succeeded, or `Deinitialize()` ran since |
| `InvalidArgument` | 2 | A `Config` field or argument is out of range (`Validate()`); nothing was sent |
| `Bus` | 3 | A port hook returned `false` |
| `Timeout` | 4 | A reset or a conversion did not finish in time |
| `DeviceIdMismatch` | 5 | CHIP_ID is not `0xA1` |
| `Crc` | 6 | The CRC-8 of a result does not match |
| `Busy` | 7 | Continuous mode runs, or a conversion is in progress (`TriggerOneShot()`) |
| `DeviceFault` | 8 | STATUS.FAULT is set |
| `Unsupported` | 9 | CRC on revision A silicon, or `HardwareReset()` without `GpioSet()` |
| `UnexpectedReset` | 16 | STATUS.POR is set during a read: the chip reset behind the driver's back. The driver is uninitialised afterwards |

The [API guide](api-guide.md#handle-errors) says what to do about each.

## Memory

No heap, no exceptions, no RTTI, no `std::function`, no `std::string`, no `printf` and no static mutable state. A
driver object holds a reference to the port, a `Config`, the revision and a flag: under 64 bytes. The largest stack
buffer is 5 bytes (one register burst).

## Platform

Platform-free: the same code runs on any target with a C++20 compiler, including the host tests. The ESP-IDF port
in [`examples/esp32`](examples.md) is an example, not part of the driver.

## Deviations from the standard

None.
