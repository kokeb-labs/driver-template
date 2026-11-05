## Wiring (ESP32-C6 DevKitC-1)

| ESP32-C6 | XMPL100 | Notes |
| --- | --- | --- |
| GPIO4 (TX) | RXD | UART1, 115200 8N1 |
| GPIO5 (RX) | TXD | |
| GPIO11 | RESET_N | Optional; set `reset_n = -1` in `Esp32Xmpl100Pins` when tied high |
| 3V3 | VDD | 100 nF to GND at the pin |
| GND | GND | |
