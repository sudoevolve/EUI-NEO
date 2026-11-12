#include "modules/serial/serial.h"
#include <type_traits>
#include <utility>
#if !defined(_WIN32)
#include <cstdlib>
#endif

int main() {
    using namespace core::serial;
    static_assert(!std::is_copy_constructible_v<SerialPort>);
    static_assert(!std::is_copy_assignable_v<SerialPort>);
    static_assert(std::is_nothrow_move_constructible_v<SerialPort>);
    SerialPort port;
    if (port.open(nullptr, 9600) || port.isOpen()) return 1;
#if defined(_WIN32)
    for (int i = 0; i < 256; ++i) {
        if (port.open("NUL", 9600) || port.isOpen()) return 2;
    }
#else
    if (port.open("/dev/null", 9600) || port.isOpen()) return 2;
    const int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) || unlockpt(master)) return 3;
    const char* device = ptsname(master);
    if (!device || !port.open(device, 9600)) return 4;
    if (port.open(device, 12345) || !port.isOpen()) return 5;
    SerialPort moved(std::move(port));
    if (port.isOpen() || !moved.isOpen()) return 6;
    port = std::move(moved);
    if (moved.isOpen() || !port.isOpen()) return 7;
    close(master);
#endif
    SerialPort finalPort(std::move(port));
    if (port.isOpen()) return 8;
    finalPort.close();
    finalPort.close();
    return finalPort.isOpen() ? 9 : 0;
}
