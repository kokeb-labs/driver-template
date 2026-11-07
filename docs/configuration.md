# Configuration

Every field of `xmpl100::Config`, the presets, how a configuration is checked, and the optional Kconfig layer.

<!-- generated: datasheet-refs -->
??? abstract "Datasheet sections this page relies on"

    From Example Corp (fictional) XMPL100-DS A:

    - Register map and reset values: 7 Register map, p. 11
    - Conversion modes, output data rate and averaging: 7.4 CTRL; 7.5 CONFIG, p. 13
    - Alert thresholds and the ALERT pin: 7.7 ALERT_HIGH and ALERT_LOW, p. 15
    - Offset correction: 7.8 OFFSET, p. 15

    All of them, with the driver API for each: [Datasheet map](datasheet.md).
<!-- /generated -->

## Fields

`Config{}` holds the chip's reset values, so it changes nothing on a freshly reset chip. Fields with a unit carry
it in the name.

| Field | Default | Range | Register |
| --- | --- | --- | --- |
| `mode` | `Mode::Continuous` | `Shutdown`, `OneShot`, `Continuous` | CTRL.MODE |
| `odr` | `OutputDataRate::Hz4` | `Hz1` to `Hz16` | CONFIG.ODR |
| `averaging` | `Averaging::X1` | `X1`, `X2`, `X4`, `X8` | CONFIG.AVG |
| `crc_enable` | `false` | revision B only | CONFIG.CRC_EN |
| `alert_enable` | `false` | | CTRL.ALERT_EN |
| `alert_polarity` | `AlertPolarity::ActiveLow` | | CTRL.ALERT_POL |
| `alert_low_c` | -40.0 | -55 °C to `alert_high_c` | ALERT_LOW |
| `alert_high_c` | 125.0 | `alert_low_c` to 150 °C | ALERT_HIGH |
| `offset_c` | 0.0 | -8 °C to +7.9375 °C, 1/16 °C steps | OFFSET |
| `conversion_timeout_ms` | 100 | 1 or more | driver only |

## Presets

| Preset | What it sets | Use it for |
| --- | --- | --- |
| `LowPowerPreset()` | 1 Hz, no averaging | Battery-powered logging |
| `HighAccuracyPreset()` | 8 Hz, 8 samples, CRC on | Control loops; needs revision B |
| `OneShotPreset()` | One-shot mode, 2 samples | Measurements on demand, shutdown in between |

Start from a preset or from `Config{}` and change fields:

```cpp
--8<-- "tests/docs/configuration.cpp:custom"
```

## Validation

`xmpl100::Validate(config)` checks a configuration without touching the chip, and `EnsureInitialized()` and
`ApplyConfig()` call it before any I/O. It returns `Error::InvalidArgument` when an enum holds a value that is not
an enumerator, a threshold or the offset is out of range, the low threshold is not below the high one, the timeout
is zero, or one averaged result takes longer than the data-rate period in continuous mode (at 16 Hz, at most 4
samples).

## Kconfig

Under ESP-IDF, `idf.py menuconfig` shows **XMPL100 temperature sensor** when the driver is a component of the
project ([Installation](installation.md)); the menu is [`_config/Kconfig`](../_config/Kconfig). The options reach
the code only through [`inc/xmpl100_kconfig.hpp`](../inc/xmpl100_kconfig.hpp), never
through a driver signature:

```cpp
--8<-- "tests/docs/configuration.cpp:kconfig"
```

| Option | Default | `Config` field |
| --- | --- | --- |
| `CONFIG_XMPL100_ODR_*` | 4 Hz | `odr` |
| `CONFIG_XMPL100_AVERAGING_SAMPLES` | 1 | `averaging` |
| `CONFIG_XMPL100_CRC_ENABLE` | n | `crc_enable` |
| `CONFIG_XMPL100_CONVERSION_TIMEOUT_MS` | 100 | `conversion_timeout_ms` |

Without Kconfig (plain CMake, other platforms) `KconfigDefaults()` returns `Config{}`. Both cases are host
tests.

Next: [Porting](porting.md).
