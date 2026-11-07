# Troubleshooting

Symptoms, causes and fixes, cheapest check first.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Example Corp (fictional) XMPL100-DS A:

    - Chip ID and silicon revision: 7.1 CHIP_ID and REV_ID, p. 11
    - Status flags: 7.3 STATUS, p. 12
    - Offset correction: 7.8 OFFSET, p. 15
    - CRC-8 of the result: 7.9 CRC, p. 16
    - Soft reset and t_RST: 8.2 Soft reset, p. 17
    - Conversion time t_CONV: 8.4 Conversion timing, p. 18

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## EnsureInitialized returns Bus

The port reported a failure: either `EnsureInitialized()` returned `false` or a transfer did.

1. Check that your port's `EnsureInitialized()` returns `true` on its own (log it).
2. Check the bus pins in your port against the board.
3. With a logic analyser, check that the first frame (the soft-reset write to CTRL) appears on the bus.

## EnsureInitialized returns DeviceIdMismatch

CHIP_ID did not read `0xA1`. The chip answered with something else, or nothing answered.

<!-- scaffold:bus-troubleshooting -->
| Read value | Likely cause | Fix |
| --- | --- | --- |
| `0x00` | SDO not connected, or CS_N not reaching the chip | Check SDO and CS_N continuity |
| `0xFF` | SDO floating high, or the chip unpowered | Check VDD and that SDO is not pulled up hard |
| Anything else | Wrong SPI mode, or a shifted bit | Use mode 0 or 3; check that CS_N stays low for the whole frame |
<!-- /scaffold:bus-troubleshooting -->

## EnsureInitialized returns Timeout

STATUS.BUSY did not clear after the reset. Check VDD (1.8 V to 3.6 V) and that RESET_N is high, or released by
your `GpioSet()` with `GpioSignal::INACTIVE`.

## EnsureInitialized returns Unsupported

`crc_enable` is set but the chip is revision A (`GetRevision()` returns `0x10`): clear `crc_enable`, or fit
revision B silicon.

## Reads return DeviceFault

STATUS.FAULT is set: the sensing element is open or shorted, usually from an overvoltage or ESD event. The chip
needs replacing; a reset does not clear it.

## Reads return Crc now and then

Single corrupted results point at signal integrity: lower the bus clock, shorten the wires, add a ground next to
the clock line. A `Crc` on every read points at a port that corrupts the third byte of a burst.

## MeasureOnceC returns Timeout

The conversion took longer than `conversion_timeout_ms`. With averaging, one result takes up to 10 ms per sample
(80 ms with `Averaging::X8`): raise the timeout. Check that your port's `DelayUs()` really waits: a delay that
returns at once makes every timeout far too short.

## Reads return UnexpectedReset

STATUS.POR was set: the chip reset since the driver started it, usually a brown-out on VDD. The driver is uninitialised
again; `EnsureInitialized()` reconfigures it. If it repeats, check the supply and its decoupling.

## The temperature reads a few degrees high

Self-heating of the board or the chip: measure the offset against a reference and set `Config::offset_c`.
