# Serial Module

Cross-platform serial port communication module.

`SerialPort` 独占系统串口资源，不可复制，可通过 `std::move` 转移所有权。`open()` 在新设备全部配置成功后才替换当前连接；失败时释放本次创建的资源，并保留原有连接（若存在）。移动后的源对象处于关闭状态。

## Usage

```cpp
#include "modules/serial/serial.h"

core::serial::SerialPort port;
auto result = port.open("COM3", 115200);
if (result) {
    port.writeString("AT\r\n");
    char reply[256];
    int n = port.readString(reply, '\n', sizeof(reply), 1000);
    port.close();
}
```

## API

The module provides `core::serial::SerialPort` for serial communication over
RS-232 / UART devices. Supports Windows (Win32 API) and Unix (POSIX termios).

### Configuration enums

- `core::serial::DataBits` — Bits5, Bits6, Bits7, Bits8, Bits16
- `core::serial::StopBits` — One, OneAndHalf, Two
- `core::serial::Parity` — None, Even, Odd, Mark, Space

### Open result

`core::serial::OpenResult` with `ok` flag and `OpenError` enum for diagnosing
open failures (DeviceNotFound, OpenFailed, SpeedNotRecognized, etc.).

### Supported baud rates

Windows: 110–256000. Linux: 110–115200 standard, up to 4 Mbit/s conditionally
(depending on kernel support).

### Modem control

DTR, RTS, CTS, DSR, DCD, RI line status query and control.
