| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `Bus` on the first transaction | No ACK at the address | Check ADDR against the address you passed, the pull-ups and VDD |
| `0x00` or `0xFF` from CHIP_ID | SDA stuck, or a different device at the address | Scan the bus; check SDA continuity |
| Anything else | Another part at the address | Check the part number on the package |
