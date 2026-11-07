# Datasheet map

The driver follows the XMPL100 datasheet. This page says which revision, where to get it, and where each part of
the driver is described in it, so you can check any behaviour against the source. Section names and page numbers
are the datasheet's own (the printed page numbers).

<!-- scaffold:remove -->
!!! note "A fictional datasheet"
    The XMPL100 is the template's fictional example part, and so is its datasheet: the document, the sections and
    the pages below are placeholders that show the format. A driver made from the template replaces them in
    `docs/datasheets.yml`.
<!-- /scaffold:remove -->

<!-- generated: datasheet-map -->
- **Datasheet: XMPL100 Digital Temperature Sensor**, Example Corp (fictional) XMPL100-DS, revision A (2026-11-06). Get
  it from the [vendor page](https://example.com/xmpl100).
  SHA-256 of the file used: `0000000000000000000000000000000000000000000000000000000000000000`.

| Topic | Datasheet section | Page | In the driver | Explained on |
| --- | --- | --- | --- | --- |
| Pins and package | 3 Pin configuration | p. 3 | `CtrlPin::Reset` | [Hardware](hardware.md), [Quickstart](quickstart.md) |
| Supply voltage and supply current | 5 Electrical characteristics | p. 5 | `Mode::Shutdown`, `Deinitialize()` | [Hardware](hardware.md) |
| SPI mode, clock and register framing | 6.2 SPI interface | p. 8 | `SpiInterface`, `SpiPort`, every register access | [Hardware](hardware.md), [Porting](porting.md), [Architecture](architecture.md) |
| Register map and reset values | 7 Register map | p. 11 | `inc/xmpl100_registers.hpp`, `Config{}` | [Configuration](configuration.md), [Architecture](architecture.md) |
| Chip ID and silicon revision | 7.1 CHIP_ID and REV_ID | p. 11 | `EnsureInitialized()`, `GetRevision()`, `Error::DeviceIdMismatch` | [Hardware](hardware.md), [Troubleshooting](troubleshooting.md) |
| Status flags | 7.3 STATUS | p. 12 | `ReadStatus()`, `Status`, `Error::DeviceFault`, `Error::UnexpectedReset` | [API guide](api-guide.md), [Troubleshooting](troubleshooting.md) |
| Conversion modes, output data rate and averaging | 7.4 CTRL; 7.5 CONFIG | p. 13 | `Config::mode`, `odr`, `averaging`, `SetMode()`, `TriggerOneShot()`, `MeasureOnceC()` | [Configuration](configuration.md), [API guide](api-guide.md) |
| Temperature result format | 7.6 TEMP_MSB, TEMP_LSB and TEMP_CRC | p. 14 | `ReadRawTemperature()`, `ReadTemperatureC()` | [API guide](api-guide.md) |
| Alert thresholds and the ALERT pin | 7.7 ALERT_HIGH and ALERT_LOW | p. 15 | `Config::alert_enable`, `alert_polarity`, `alert_low_c`, `alert_high_c`, `SetAlertThresholdsC()` | [API guide](api-guide.md), [Configuration](configuration.md), [Hardware](hardware.md) |
| Offset correction | 7.8 OFFSET | p. 15 | `Config::offset_c` | [Configuration](configuration.md), [Troubleshooting](troubleshooting.md) |
| CRC-8 of the result | 7.9 CRC | p. 16 | `Config::crc_enable`, `reg::Crc8()`, `Error::Crc` | [Hardware](hardware.md), [Troubleshooting](troubleshooting.md) |
| Power-up time | 8.1 Power-up | p. 17 | - | [Hardware](hardware.md) |
| Soft reset and t_RST | 8.2 Soft reset | p. 17 | `EnsureInitialized()`, `SoftReset()`, `kResetTimeMaxUs`, `Error::Timeout` | [Hardware](hardware.md), [Porting](porting.md), [Troubleshooting](troubleshooting.md) |
| Hardware reset (RESET_N) and t_RST_PW | 8.3 Hardware reset | p. 18 | `HardwareReset()`, `CtrlPin::Reset`, `kResetPulseMinUs` | [Hardware](hardware.md), [Porting](porting.md) |
| Conversion time t_CONV | 8.4 Conversion timing | p. 18 | `kConversionTimeMaxUs`, `Config::conversion_timeout_ms`, `WaitDataReady()` | [Hardware](hardware.md), [Troubleshooting](troubleshooting.md) |
<!-- /generated -->

## When the datasheet and the driver differ

The driver is written against the revision above. If you have a newer revision, compare its revision history with
the topics above; a change to a pin, a register, a timing or a value the driver uses is a bug report for this
repository. Where the driver deliberately goes beyond the datasheet, the [Contract](contract.md) says so.

The repository never contains the PDF itself: vendors own their documents. Download it from the vendor, and compare
its SHA-256 with the one above to confirm you have the same file.
