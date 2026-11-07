## What you implement

| Method | Required | Contract |
| --- | --- | --- |
| `bool EnsureInitialized() noexcept` | yes | Bring the UART up on the first call; return `true` at once afterwards. `false` makes the driver's `EnsureInitialized()` return `Bus` |
| `bool Write(const uint8_t* data, std::size_t len) noexcept` | yes | Send all `len` bytes; `false` on an error |
| `bool Read(uint8_t* data, std::size_t len, uint32_t timeout_ms) noexcept` | yes | Receive exactly `len` bytes within `timeout_ms`, or return `false` |
| `bool FlushInput() noexcept` | yes | Drop every received byte not read yet |
| `void DelayUs(uint32_t us) noexcept` | yes, for every wait | Wait at least `us` microseconds |
| `bool GpioSet(CtrlPin, GpioSignal) noexcept` | no | Drive RESET_N; `false` if the pin write failed |
| `void Log(LogLevel, const char* message) noexcept` | no | Print one line; `message` is plain text, never a format string |

## The port

```cpp title="my_port.hpp"
--8<-- "tests/docs/my_port.hpp:port"
```

Rules the port must follow:

- **Raw bytes only.** The driver builds the frames and checks their CRC; the port does not parse or retry.
- **`Read()` is all or nothing**: exactly `len` bytes, or `false` when the time-out expires.
- **`FlushInput()` really flushes**: the driver calls it before each request so a stale byte cannot shift a reply.
- **115200 baud, 8N1**, no flow control.
