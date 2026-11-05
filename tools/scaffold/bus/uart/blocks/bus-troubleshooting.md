| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `Timeout` on the first request | No reply: TX and RX swapped, or the wrong baud rate | Cross TX and RX; 115200 8N1 |
| `Crc` now and then | Noise, or a baud-rate error above 2 % | Shorten the wires; check the UART clock |
| `Bus` | A reply that does not start with `0xA5` | Another device on the line, or a stale byte: check `FlushInput()` |
