| Bus pin | Direction | Function |
| --- | --- | --- |
| SCL | In | I2C clock; needs a pull-up |
| SDA | In/out | I2C data, open drain; needs a pull-up |
| ADDR | In | Address select: GND `0x48`, VDD `0x49`, SDA `0x4A`, SCL `0x4B` |
