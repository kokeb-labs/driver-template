**2. Write the port**: a class with `EnsureInitialized()` (bring the UART up), `Write()`, `Read()` with a time-out,
`FlushInput()` and `DelayUs()`, about 30 lines on most SDKs. [Porting](docs/porting.md) has the contract and a
complete example.
