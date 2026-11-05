## Wiring (ESP32-C6 DevKitC-1)

| ESP32-C6 | XMPL100 | Notes |
| --- | --- | --- |
| GPIO4 | OUT | Open drain: 10 kΩ pull-up to 3V3 (or the internal pull-up the port enables) |
| GPIO11 | SHDN_N | Optional; set `shdn_n = -1` in `Esp32Xmpl100Pins` when tied high |
| 3V3 | VDD | 100 nF to GND at the pin |
| GND | GND | |
