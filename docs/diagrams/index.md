# Figures

<!-- Written by kokeb-docs figures from docs/diagrams/figures/*.py. Do not edit by hand. -->

Every figure on this site is drawn by a Python module from facts extracted from the code, so it is rebuilt,
not redrawn, when the code changes. CI runs `kokeb-docs figures --check` and fails when a
committed figure no longer matches the code. Each figure below lists the files it is drawn from.

## Inside the XMPL100

<!-- figure: chip-blocks -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-blocks.dark.svg">
  <img src="chip-blocks.svg"
    alt="Block diagram of the XMPL100: its function blocks, how they connect and which package pins reach
    each block.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)

## What the XMPL100 does

<!-- figure: chip-capabilities -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-capabilities.dark.svg">
  <img src="chip-capabilities.svg"
    alt="Capability map of the XMPL100: Temperature (supported): Measures the die temperature as a 16-bit
    result, 1/128 °C per count; Continuous and one-shot (supported): Converts continuously at a chosen
    rate, or once on request to save power; Alert window (supported): Pulls the open-drain ALERT pin low
    outside a high and a low threshold; Result CRC (supported): Protects each result with a CRC-8 on
    revision B silicon; Offset trim (supported): Shifts every result by a signed offset, 1/16 °C per
    count; Reset and identity (supported): Soft or hardware reset, then a chip ID and revision check
    before anything else.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)

## XMPL100: Typical application

<!-- figure: chip-circuit-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-circuit-typical.dark.svg">
  <img src="chip-circuit-typical.svg"
    alt="Schematic of the XMPL100 typical application: the chip with U2 MCU, C1 capacitor 100 nF, R1 resistor
    10 kΩ; the MCU wires carry the port hooks GpioSet(CtrlPin::Reset), Transfer().">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)

## XMPL100: SPI register access

<!-- figure: chip-frame-spi -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-frame-spi.dark.svg">
  <img src="chip-frame-spi.svg"
    alt="SPI register access of the XMPL100, 16 bits, most significant bit first. Write: R/W (bit 15),
    address (bits 14 to 8), data (bits 7 to 0); Read: R/W (bit 15), address (bits 14 to 8), any value
    (bits 7 to 0); Reply: ignored (bits 15 to 8), register data (bits 7 to 0). Below, the SPI mode 0
    waveform of one transfer in the same bit grid.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)

## XMPL100

<!-- figure: chip-hero -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-hero.dark.svg">
  <img src="chip-hero.svg"
    alt="Overview of the XMPL100, a digital temperature sensor that measures its own die from -40 °C to +125
    °C in steps of 1/128 °C and pulls an alert line low when the temperature leaves a window you set. A
    MCU talks to it over SPI; it drives ALERT line; it reads back Die temperature. Headline figures: -40
    to +125 °C measurement range; 1/128 °C resolution of the 16-bit result; 10 MHz SPI clock, mode 0 or
    3; 1.8 to 3.6 V supply.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)

## XMPL100 package, SOIC-8

<!-- figure: chip-outline -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-outline.dark.svg">
  <img src="chip-outline.svg"
    alt="Outline of the XMPL100 in SOIC-8 drawn to scale from above: body 4.9 x 3.9 x 1.75 mm, pitch 1.27 mm,
    8 pins.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)

## XMPL100 pinout, SOIC-8

<!-- figure: chip-pinout -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-pinout.dark.svg">
  <img src="chip-pinout.svg"
    alt="Pinout of the XMPL100 in SOIC-8, top view: 8 pins, each with its name, its electrical type as a
    colour and the driver hook that uses it.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)

## Wiring: Typical application

<!-- figure: chip-wiring-typical -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="chip-wiring-typical.dark.svg">
  <img src="chip-wiring-typical.svg"
    alt="Wiring of the XMPL100 for the Typical application: MCU SPI SCK to SCLK (Transfer()); MCU SPI MOSI to
    SDI (Transfer()); MCU SPI MISO to SDO (Transfer()); MCU GPIO / CS to CS_N (Transfer()); MCU GPIO to
    RESET_N (GpioSet(CtrlPin::Reset)); MCU GPIO (IRQ) to ALERT. Supplies, grounds and the support parts
    are listed under the drawing.">
</picture>
<!-- /figure -->

Hardware description: [`docs/hardware.yml`](../hardware.yml)

Drawn from:

- [`docs/hardware.yml`](../hardware.yml)

## The layers, one of them yours

<!-- figure: driver-layers -->
<picture>
  <source media="(prefers-color-scheme: dark)" srcset="driver-layers.dark.svg">
  <img src="driver-layers.svg"
    alt="Layer diagram: your application calls the XMPL100 core, which sends register reads and writes
    through the SPI port contract to the port class you write, which drives the chip; the core and the
    contract are the same on every MCU.">
</picture>
<!-- /figure -->

Figure module: [`docs/diagrams/figures/driver-layers.py`](../../docs/diagrams/figures/driver-layers.py)

Drawn from:

- `public calls` in [`inc/xmpl100.hpp`](../../inc/xmpl100.hpp)
- `SpiInterface` in [`inc/xmpl100_spi_interface.hpp`](../../inc/xmpl100_spi_interface.hpp)
