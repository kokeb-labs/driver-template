## UART timing

The XMPL100 talks 8N1 on one UART: a start bit, eight data bits least significant first, and a stop bit.

<!-- figure: bus-timing -->
<!-- /figure -->

## Several devices

When the datasheet describes several devices on one line (a multi-drop bus with addresses, RS-485), draw it as a
`topology` figure: `kokeb-docs figkit new --kind topology --id bus-topology --page docs/bus.md`.
