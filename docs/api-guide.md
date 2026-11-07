# API guide

The driver's API by task, with code that compiles: initialise, configure, measure, handle errors and shut down.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Example Corp (fictional) XMPL100-DS A:

    - Status flags: 7.3 STATUS, p. 12
    - Conversion modes, output data rate and averaging: 7.4 CTRL; 7.5 CONFIG, p. 13
    - Temperature result format: 7.6 TEMP_MSB, TEMP_LSB and TEMP_CRC, p. 14
    - Alert thresholds and the ALERT pin: 7.7 ALERT_HIGH and ALERT_LOW, p. 15

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## The shape of the API

| Group | Calls |
| --- | --- |
| Lifecycle | `EnsureInitialized()`, `IsInitialized()`, `Deinitialize()`, `SoftReset()`, `HardwareReset()` |
| Configuration | `ApplyConfig()`, `GetConfig()`, `SetMode()`, `SetAlertThresholdsC()` |
| Measurement | `ReadTemperatureC()`, `ReadRawTemperature()`, `ReadStatus()`, `TriggerOneShot()`, `WaitDataReady()`, `MeasureOnceC()` |
| Identification | `GetRevision()`, `xmpl100::GetDriverVersion()` |

Every call that touches the chip returns `Result<T>`: check `ok()` (or the `bool` conversion) and read `value`, or
read `error`. `Result` is `[[nodiscard]]`, so an unchecked call is a compiler warning.

## Initialise

```cpp
--8<-- "tests/docs/api_tasks.cpp:lifecycle"
```

The constructor stores the port and the configuration (`Config{}` unless you pass one) and does no I/O.
`EnsureInitialized()` is the one way to start the chip. Its first call brings the bus up, resets the chip (RESET_N
when the port has `GpioSet()`, otherwise a soft reset), checks CHIP_ID and writes the configuration.

Once it has succeeded, `EnsureInitialized()` returns `Ok` at once with no I/O, so a caller (or a handler that owns
the driver) can call it before every use. After a failure the driver stays uninitialised and the next call starts
again from the beginning. No other call starts the chip: before `EnsureInitialized()` succeeds every read and write
returns `NotInitialized` and sends nothing. `ApplyConfig()`, `SetMode()` and `SetAlertThresholdsC()` before the
start only store the configuration for the first `EnsureInitialized()`.

## Measure on demand

In `Mode::OneShot` (or `Shutdown`) the chip converts only when asked, then returns to shutdown:

```cpp
--8<-- "tests/docs/api_tasks.cpp:one-shot"
```

`MeasureOnceC()` is `TriggerOneShot()`, `WaitDataReady()` and `ReadTemperatureC()` in one call. The wait is
bounded by `Config::conversion_timeout_ms` (default 100 ms; one result takes up to 10 ms per averaged sample).

## Read continuously

In `Mode::Continuous` (the reset value) the chip converts at the output data rate and `ReadTemperatureC()` returns
the latest result. Read STATUS first when you need to know whether the result is new:
`ReadStatus().value.data_ready` is set until the result is read.

## Alerts

```cpp
--8<-- "tests/docs/api_tasks.cpp:alerts"
```

The chip compares each result with the thresholds and drives ALERT; `ReadStatus()` says which threshold was
crossed (`alert_high`, `alert_low`).

## Handle errors

```cpp
--8<-- "tests/docs/api_tasks.cpp:errors"
```

| Signal | Meaning | What to do |
| --- | --- | --- |
| `Error::Bus` | The port failed a transfer or the bus bring-up | Check the port and the wiring; retry, `SoftReset()`, or `Deinitialize()` and `EnsureInitialized()` |
| `Error::Timeout` | Reset or conversion did not finish | Check VDD and RESET_N; raise `conversion_timeout_ms` with heavy averaging |
| `Error::DeviceFault` / `Status::fault` | The sensing element is open or shorted | Replace the part |
| `Error::Crc` | A result was corrupted on the bus | Lower the bus clock; check grounding |
| `Error::UnexpectedReset` | The chip reset behind the driver's back | `EnsureInitialized()` reconfigures it |
| `Error::Unsupported` | CRC on revision A, or `HardwareReset()` without `GpioSet()` | Clear `crc_enable`, or add the hook |

Every value, with the calls that return it, is on the [Contract](contract.md#errors) page.
`xmpl100::ToString()` names an error for a log.

## Shut down

```cpp
--8<-- "tests/docs/api_tasks.cpp:deinit"
```

After `Deinitialize()` every call returns `NotInitialized` until the next `EnsureInitialized()`, which starts the
chip again from the beginning: `Deinitialize()` then `EnsureInitialized()` is also the recovery after a fault.

Next: [Configuration](configuration.md).
