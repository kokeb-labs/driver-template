# API guide

The driver's API by task, with code that compiles: initialise, configure, measure, handle errors and shut down.

## The shape of the API

| Group | Calls |
| --- | --- |
| Lifecycle | `Initialize()`, `EnsureInitialized()`, `IsInitialized()`, `Deinitialize()`, `SoftReset()`, `HardwareReset()` |
| Configuration | `ApplyConfig()`, `GetConfig()`, `SetMode()`, `SetAlertThresholdsC()` |
| Measurement | `ReadTemperatureC()`, `ReadRawTemperature()`, `ReadStatus()`, `TriggerOneShot()`, `WaitDataReady()`, `MeasureOnceC()` |
| Identification | `GetRevision()`, `xmpl100::GetDriverVersion()` |

Every call that touches the chip returns `Result<T>`: check `ok()` (or the `bool` conversion) and read `value`, or
read `error`. `Result` is `[[nodiscard]]`, so an unchecked call is a compiler warning.

## Initialise

```cpp
--8<-- "tests/docs/api_tasks.cpp:lifecycle"
```

`Initialize()` is safe to call again: it resets and reconfigures the chip each time. `EnsureInitialized()` runs it
only when the driver is not initialised yet, with the last configuration.

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
| `Error::Bus` | The port failed a transfer | Check the port and the wiring; retry or `SoftReset()` |
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

After `Deinitialize()` every call returns `NotInitialized` until the next `Initialize()`.

Next: [Configuration](configuration.md).
