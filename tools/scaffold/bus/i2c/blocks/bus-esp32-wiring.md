## Wiring (ESP32-C6 DevKitC-1)

| ESP32-C6 | XMPL100 | Notes |
| --- | --- | --- |
| GPIO6 | SDA | I2C0, 100 kHz; internal pull-ups enabled, add 4.7 kΩ for long wires |
| GPIO7 | SCL | |
| GND | ADDR | Address `0x48` |
| GPIO11 | RESET_N | Optional; set `reset_n = -1` in `Esp32Xmpl100Pins` when tied high |
| 3V3 | VDD | 100 nF to GND at the pin |
| GND | GND | |
