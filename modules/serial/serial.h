#pragma once

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#if defined(__linux__) || defined(__APPLE__)
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#endif

#include <string>

namespace core::serial {

// -----------------------------------------------------------------------------
// Data bits configuration
// -----------------------------------------------------------------------------
enum class DataBits {
    Bits5,
    Bits6,
    Bits7,
    Bits8,
    Bits16
};

// -----------------------------------------------------------------------------
// Stop bits configuration
// -----------------------------------------------------------------------------
enum class StopBits {
    One,
    OneAndHalf,
    Two
};

// -----------------------------------------------------------------------------
// Parity configuration
// -----------------------------------------------------------------------------
enum class Parity {
    None,
    Even,
    Odd,
    Mark,
    Space
};

// -----------------------------------------------------------------------------
// Open error codes
// -----------------------------------------------------------------------------
enum class OpenError {
    None,
    DeviceNotFound,
    OpenFailed,
    PortParamsFailed,
    SpeedNotRecognized,
    WriteParamsFailed,
    TimeoutParamsFailed,
    DatabitsNotRecognized,
    StopbitsNotRecognized,
    ParityNotRecognized
};

// -----------------------------------------------------------------------------
// Open result
// -----------------------------------------------------------------------------
struct OpenResult {
    bool ok = false;
    OpenError error = OpenError::None;

    explicit operator bool() const { return ok; }
};

// -----------------------------------------------------------------------------
// SerialPort — cross-platform serial communication
// -----------------------------------------------------------------------------
class SerialPort {
public:
    SerialPort();
    ~SerialPort();

    // Open a serial device
    // Returns OpenResult with ok=true on success, or an error code on failure.
    OpenResult open(const char* device, unsigned int bauds,
                    DataBits databits = DataBits::Bits8,
                    Parity parity = Parity::None,
                    StopBits stopbits = StopBits::One);

    // Check if device is open
    bool isOpen();

    // Close the device
    void close();

    // Write a single byte
    // Returns 1 on success, -1 on error.
    int writeChar(char byte);

    // Read a single byte (with timeout)
    // Returns 1 on success, 0 on timeout, -1 on error.
    int readChar(char* byte, unsigned int timeoutMs = 0);

    // Write a null-terminated string
    // Returns true on success, false on error.
    bool writeString(const char* str);

    // Read a string until finalChar is encountered (with timeout)
    // Returns number of bytes read (including null) on success,
    // 0 on timeout, negative on error.
    int readString(char* str, char finalChar, unsigned int maxBytes,
                   unsigned int timeoutMs = 0);

    // Write an array of bytes
    // Returns true on success, false on error.
    bool writeBytes(const void* buffer, unsigned int count);

    // Read an array of bytes (with timeout)
    // Returns number of bytes read on success, negative on error.
    int readBytes(void* buffer, unsigned int maxBytes,
                  unsigned int timeoutMs = 0,
                  unsigned int sleepDurationUs = 100);

    // Empty the receive buffer
    bool flushReceiver();

    // Return the number of bytes in the receive buffer
    int available();

    // Set or clear DTR (pin 4)
    bool setDtr(bool status);
    bool setDtr();
    bool clearDtr();

    // Set or clear RTS (pin 7)
    bool setRts(bool status);
    bool setRts();
    bool clearRts();

    // Get modem line status
    bool isCts();   // Clear To Send (pin 8)
    bool isDsr();   // Data Set Ready (pin 6)
    bool isDcd();   // Data Carrier Detect (pin 1)
    bool isRi();    // Ring Indicator (pin 9)
    bool isDtr();   // Data Terminal Ready (pin 4)
    bool isRts();   // Request To Send (pin 7)

private:
    // Read a string without timeout
    int readStringNoTimeout(char* str, char finalChar, unsigned int maxBytes);

    // Current DTR and RTS state (can't be read on Windows)
    bool currentStateRts_ = true;
    bool currentStateDtr_ = true;

#if defined(_WIN32) || defined(_WIN64)
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    COMMTIMEOUTS timeouts_;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int fd_ = -1;
#endif
};

} // namespace core::serial