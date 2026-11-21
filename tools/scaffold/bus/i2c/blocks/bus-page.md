## I2C timing

The XMPL100 is an I2C target at the address its ADDR pin selects. Data changes while SCL is low and is read while SCL
is high; a start or a stop is SDA changing while SCL is high.

<!-- figure: bus-timing -->
<!-- /figure -->

## Several devices

Up to four XMPL100 share one bus, each at the address its ADDR pin selects.

<!-- figure: bus-topology -->
<!-- /figure -->
