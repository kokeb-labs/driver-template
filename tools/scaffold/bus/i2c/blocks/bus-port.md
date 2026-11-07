## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the bus up on the first call; return `true` at once afterwards. `false` makes the driver's `EnsureInitialized()` return `Bus` |
| `bool Write(uint8_t addr, const uint8_t* data, std::size_t len) noexcept` | yes | START, `addr` + W, `len` bytes, STOP. `false` on a NACK or any bus error |
| `bool WriteRead(uint8_t addr, const uint8_t* tx, std::size_t tx_len, uint8_t* rx, std::size_t rx_len) noexcept` | yes | START, `addr` + W, `tx`, repeated START, `addr` + R, `rx_len` bytes, STOP |
| `void DelayUs(uint32_t us) noexcept` | yes, for every wait | Wait at least `us` microseconds |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | no | Drive RESET_N; `false` if the pin write failed |
| `void Log(LogLevel, const char* fmt, ...) noexcept` | no | Format and print one line; the driver never formats |

`addr` is the 7-bit address the driver was constructed with; shift it yourself if your SDK wants the 8-bit form.

## The port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

Rules the transfers must follow:

- **One call, one transaction.** The register address is the first byte of `Write()` and the `tx` of
  `WriteRead()`; never split a burst, the address auto-increment does not carry over.
- **Repeated START for `WriteRead()`**, no STOP between the write and the read.
- **Raw bytes only.** Do not retry; the driver owns the framing.
- **Return `false` for a NACK or a bus error.** The driver turns it into `Error::Bus`.
- **Bounded time-outs.** A stuck bus fails the call; it does not block forever.
