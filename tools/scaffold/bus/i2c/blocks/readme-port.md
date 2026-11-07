**2. Write the port**: a class with `EnsureInitialized()` (bring the bus up), `Write()` and `WriteRead()` (one I2C
transaction each) and `DelayUs()`, about 20 lines on most SDKs. [Porting](docs/porting.md) has the contract and a
complete example.
