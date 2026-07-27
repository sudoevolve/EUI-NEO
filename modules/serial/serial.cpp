#include "serial.h"

#include <cstring>

#if defined(_WIN32) || defined(_WIN64)
#if defined(__GNUC__)
// MinGW provides sys/time.h for gettimeofday
#include <sys/time.h>
#else
// sys/time.h does not exist on "actual" Windows
#define NO_POSIX_TIME
#endif
#endif

namespace core::serial {

// =============================================================================
// TimeoutHelper — internal timer for read operations
// =============================================================================

namespace {

class TimeoutHelper {
public:
    TimeoutHelper() = default;

    void initTimer() {
#if defined(NO_POSIX_TIME)
        LARGE_INTEGER tmp;
        QueryPerformanceFrequency(&tmp);
        counterFrequency_ = tmp.QuadPart;
        QueryPerformanceCounter(&tmp);
        previousTime_ = tmp.QuadPart;
#else
        gettimeofday(&previousTime_, nullptr);
#endif
    }

    unsigned long int elapsedTimeMs() {
#if defined(NO_POSIX_TIME)
        LARGE_INTEGER currentTime;
        QueryPerformanceCounter(&currentTime);
        long long sec = currentTime.QuadPart - previousTime_;
        return static_cast<unsigned long int>(sec / (counterFrequency_ / 1000));
#else
        struct timeval currentTime;
        gettimeofday(&currentTime, nullptr);
        int sec = currentTime.tv_sec - previousTime_.tv_sec;
        int usec = currentTime.tv_usec - previousTime_.tv_usec;
        if (usec < 0) {
            usec = 1000000 - previousTime_.tv_usec + currentTime.tv_usec;
            sec--;
        }
        return static_cast<unsigned long int>(sec * 1000 + usec / 1000);
#endif
    }

private:
#if defined(NO_POSIX_TIME)
    LONGLONG counterFrequency_ = 0;
    LONGLONG previousTime_ = 0;
#else
    struct timeval previousTime_ = {};
#endif
};

} // anonymous namespace

// =============================================================================
// Constructor and destructor
// =============================================================================

SerialPort::SerialPort()
#if defined(_WIN32) || defined(_WIN64)
    : currentStateRts_(true), currentStateDtr_(true), handle_(INVALID_HANDLE_VALUE)
#endif
#if defined(__linux__) || defined(__APPLE__)
    : fd_(-1)
#endif
{
}

SerialPort::~SerialPort() {
    close();
}

// =============================================================================
// Configuration and initialization
// =============================================================================

OpenResult SerialPort::open(const char* device, unsigned int bauds,
                            DataBits databits, Parity parity,
                            StopBits stopbits) {
#if defined(_WIN32) || defined(_WIN64)
    // Open serial port
    handle_ = CreateFileA(device,
                          GENERIC_READ | GENERIC_WRITE,
                          0, nullptr,
                          OPEN_EXISTING,
                          0, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
            return {false, OpenError::DeviceNotFound};
        return {false, OpenError::OpenFailed};
    }

    // Get current port parameters
    DCB dcbSerialParams;
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(handle_, &dcbSerialParams))
        return {false, OpenError::PortParamsFailed};

    // Set baud rate
    switch (bauds) {
        case 110:    dcbSerialParams.BaudRate = CBR_110; break;
        case 300:    dcbSerialParams.BaudRate = CBR_300; break;
        case 600:    dcbSerialParams.BaudRate = CBR_600; break;
        case 1200:   dcbSerialParams.BaudRate = CBR_1200; break;
        case 2400:   dcbSerialParams.BaudRate = CBR_2400; break;
        case 4800:   dcbSerialParams.BaudRate = CBR_4800; break;
        case 9600:   dcbSerialParams.BaudRate = CBR_9600; break;
        case 14400:  dcbSerialParams.BaudRate = CBR_14400; break;
        case 19200:  dcbSerialParams.BaudRate = CBR_19200; break;
        case 38400:  dcbSerialParams.BaudRate = CBR_38400; break;
        case 56000:  dcbSerialParams.BaudRate = CBR_56000; break;
        case 57600:  dcbSerialParams.BaudRate = CBR_57600; break;
        case 115200: dcbSerialParams.BaudRate = CBR_115200; break;
        case 128000: dcbSerialParams.BaudRate = CBR_128000; break;
        case 256000: dcbSerialParams.BaudRate = CBR_256000; break;
        default:     return {false, OpenError::SpeedNotRecognized};
    }

    // Set data bits
    BYTE bytesize = 0;
    switch (databits) {
        case DataBits::Bits5:  bytesize = 5; break;
        case DataBits::Bits6:  bytesize = 6; break;
        case DataBits::Bits7:  bytesize = 7; break;
        case DataBits::Bits8:  bytesize = 8; break;
        case DataBits::Bits16: bytesize = 16; break;
        default:               return {false, OpenError::DatabitsNotRecognized};
    }

    // Set stop bits
    BYTE stopBits = 0;
    switch (stopbits) {
        case StopBits::One:        stopBits = ONESTOPBIT; break;
        case StopBits::OneAndHalf: stopBits = ONE5STOPBITS; break;
        case StopBits::Two:        stopBits = TWOSTOPBITS; break;
        default:                   return {false, OpenError::StopbitsNotRecognized};
    }

    // Set parity
    BYTE parityBits = 0;
    switch (parity) {
        case Parity::None:  parityBits = NOPARITY; break;
        case Parity::Even:  parityBits = EVENPARITY; break;
        case Parity::Odd:   parityBits = ODDPARITY; break;
        case Parity::Mark:  parityBits = MARKPARITY; break;
        case Parity::Space: parityBits = SPACEPARITY; break;
        default:            return {false, OpenError::ParityNotRecognized};
    }

    dcbSerialParams.ByteSize = bytesize;
    dcbSerialParams.StopBits = stopBits;
    dcbSerialParams.Parity = parityBits;

    // Write parameters
    if (!SetCommState(handle_, &dcbSerialParams))
        return {false, OpenError::WriteParamsFailed};

    // Set timeout parameters
    timeouts_.ReadIntervalTimeout = 0;
    timeouts_.ReadTotalTimeoutConstant = MAXDWORD;
    timeouts_.ReadTotalTimeoutMultiplier = 0;
    timeouts_.WriteTotalTimeoutConstant = MAXDWORD;
    timeouts_.WriteTotalTimeoutMultiplier = 0;

    if (!SetCommTimeouts(handle_, &timeouts_))
        return {false, OpenError::TimeoutParamsFailed};

    return {true, OpenError::None};
#endif
#if defined(__linux__) || defined(__APPLE__)
    // Open device
    fd_ = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd_ == -1)
        return {false, OpenError::OpenFailed};

    // Set nonblocking mode
    fcntl(fd_, F_SETFL, FNDELAY);

    // Get and clear current options
    struct termios options;
    tcgetattr(fd_, &options);
    bzero(&options, sizeof(options));

    // Set baud rate
    speed_t speed;
    switch (bauds) {
        case 110:    speed = B110; break;
        case 300:    speed = B300; break;
        case 600:    speed = B600; break;
        case 1200:   speed = B1200; break;
        case 2400:   speed = B2400; break;
        case 4800:   speed = B4800; break;
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 57600:  speed = B57600; break;
        case 115200: speed = B115200; break;
#if defined(B230400)
        case 230400:   speed = B230400; break;
#endif
#if defined(B460800)
        case 460800:   speed = B460800; break;
#endif
#if defined(B500000)
        case 500000:   speed = B500000; break;
#endif
#if defined(B576000)
        case 576000:   speed = B576000; break;
#endif
#if defined(B921600)
        case 921600:   speed = B921600; break;
#endif
#if defined(B1000000)
        case 1000000:  speed = B1000000; break;
#endif
#if defined(B1152000)
        case 1152000:  speed = B1152000; break;
#endif
#if defined(B1500000)
        case 1500000:  speed = B1500000; break;
#endif
#if defined(B2000000)
        case 2000000:  speed = B2000000; break;
#endif
#if defined(B2500000)
        case 2500000:  speed = B2500000; break;
#endif
#if defined(B3000000)
        case 3000000:  speed = B3000000; break;
#endif
#if defined(B3500000)
        case 3500000:  speed = B3500000; break;
#endif
#if defined(B4000000)
        case 4000000:  speed = B4000000; break;
#endif
        default:       return {false, OpenError::SpeedNotRecognized};
    }

    // Set data bits
    int databitsFlag = 0;
    switch (databits) {
        case DataBits::Bits5: databitsFlag = CS5; break;
        case DataBits::Bits6: databitsFlag = CS6; break;
        case DataBits::Bits7: databitsFlag = CS7; break;
        case DataBits::Bits8: databitsFlag = CS8; break;
        default:              return {false, OpenError::DatabitsNotRecognized};
    }

    // Set stop bits
    int stopbitsFlag = 0;
    switch (stopbits) {
        case StopBits::One: stopbitsFlag = 0; break;
        case StopBits::Two: stopbitsFlag = CSTOPB; break;
        default:            return {false, OpenError::StopbitsNotRecognized};
    }

    // Set parity
    int parityFlag = 0;
    switch (parity) {
        case Parity::None: parityFlag = 0; break;
        case Parity::Even: parityFlag = PARENB; break;
        case Parity::Odd:  parityFlag = (PARENB | PARODD); break;
        default:           return {false, OpenError::ParityNotRecognized};
    }

    // Apply settings
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    options.c_cflag |= (CLOCAL | CREAD | databitsFlag | parityFlag | stopbitsFlag);
    options.c_iflag |= (IGNPAR | IGNBRK);
    options.c_cc[VTIME] = 0;
    options.c_cc[VMIN] = 0;
    tcsetattr(fd_, TCSANOW, &options);

    return {true, OpenError::None};
#endif
}

bool SerialPort::isOpen() {
#if defined(_WIN32) || defined(_WIN64)
    return handle_ != INVALID_HANDLE_VALUE;
#endif
#if defined(__linux__) || defined(__APPLE__)
    return fd_ >= 0;
#endif
}

void SerialPort::close() {
#if defined(_WIN32) || defined(_WIN64)
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
#endif
#if defined(__linux__) || defined(__APPLE__)
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
#endif
}

// =============================================================================
// Read/Write operations on characters
// =============================================================================

int SerialPort::writeChar(char byte) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD dwBytesWritten;
    if (!WriteFile(handle_, &byte, 1, &dwBytesWritten, nullptr))
        return -1;
    return 1;
#endif
#if defined(__linux__) || defined(__APPLE__)
    if (write(fd_, &byte, 1) != 1)
        return -1;
    return 1;
#endif
}

int SerialPort::readChar(char* byte, unsigned int timeoutMs) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD dwBytesRead = 0;
    timeouts_.ReadTotalTimeoutConstant = timeoutMs;
    if (!SetCommTimeouts(handle_, &timeouts_))
        return -1;
    if (!ReadFile(handle_, byte, 1, &dwBytesRead, nullptr))
        return -2;
    if (dwBytesRead == 0)
        return 0;
    return 1;
#endif
#if defined(__linux__) || defined(__APPLE__)
    TimeoutHelper timer;
    timer.initTimer();
    while (timer.elapsedTimeMs() < timeoutMs || timeoutMs == 0) {
        switch (read(fd_, byte, 1)) {
            case 1:  return 1;
            case -1: return -2;
        }
    }
    return 0;
#endif
}

// =============================================================================
// Read/Write operations on strings
// =============================================================================

bool SerialPort::writeString(const char* str) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD dwBytesWritten;
    if (!WriteFile(handle_, str, static_cast<DWORD>(strlen(str)),
                   &dwBytesWritten, nullptr))
        return false;
    return true;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int len = static_cast<int>(strlen(str));
    if (write(fd_, str, len) != len)
        return false;
    return true;
#endif
}

int SerialPort::readString(char* str, char finalChar, unsigned int maxBytes,
                           unsigned int timeoutMs) {
    // No timeout requested — delegate to the no-timeout path
    if (timeoutMs == 0)
        return readStringNoTimeout(str, finalChar, maxBytes);

    unsigned int nbBytes = 0;
    int charRead;
    TimeoutHelper timer;
    timer.initTimer();

    while (nbBytes < maxBytes) {
        // Compute remaining time
        long int remaining = static_cast<long int>(timeoutMs) -
                             static_cast<long int>(timer.elapsedTimeMs());

        if (remaining > 0) {
            charRead = readChar(&str[nbBytes], static_cast<unsigned int>(remaining));

            if (charRead == 1) {
                if (str[nbBytes] == finalChar) {
                    str[++nbBytes] = 0;
                    return static_cast<int>(nbBytes);
                }
                nbBytes++;
            }

            if (charRead < 0)
                return charRead;
        }

        // Check if timeout reached
        if (timer.elapsedTimeMs() > timeoutMs) {
            str[nbBytes] = 0;
            return 0;
        }
    }

    // Buffer full
    return -3;
}

// =============================================================================
// Read/Write operations on bytes
// =============================================================================

bool SerialPort::writeBytes(const void* buffer, unsigned int count) {
#if defined(_WIN32) || defined(_WIN64)
    DWORD dwBytesWritten;
    if (!WriteFile(handle_, buffer, static_cast<DWORD>(count),
                   &dwBytesWritten, nullptr))
        return false;
    return true;
#endif
#if defined(__linux__) || defined(__APPLE__)
    if (write(fd_, buffer, count) != static_cast<ssize_t>(count))
        return false;
    return true;
#endif
}

int SerialPort::readBytes(void* buffer, unsigned int maxBytes,
                          unsigned int timeoutMs,
                          unsigned int sleepDurationUs) {
#if defined(_WIN32) || defined(_WIN64)
    (void)sleepDurationUs;
    DWORD dwBytesRead = 0;
    timeouts_.ReadTotalTimeoutConstant = static_cast<DWORD>(timeoutMs);
    if (!SetCommTimeouts(handle_, &timeouts_))
        return -1;
    if (!ReadFile(handle_, buffer, static_cast<DWORD>(maxBytes),
                  &dwBytesRead, nullptr))
        return -2;
    return static_cast<int>(dwBytesRead);
#endif
#if defined(__linux__) || defined(__APPLE__)
    TimeoutHelper timer;
    timer.initTimer();
    unsigned int nbBytesRead = 0;

    while (timer.elapsedTimeMs() < timeoutMs || timeoutMs == 0) {
        unsigned char* ptr = static_cast<unsigned char*>(buffer) + nbBytesRead;
        int ret = static_cast<int>(read(fd_, ptr, maxBytes - nbBytesRead));

        if (ret == -1)
            return -2;

        if (ret > 0) {
            nbBytesRead += static_cast<unsigned int>(ret);
            if (nbBytesRead >= maxBytes)
                return static_cast<int>(nbBytesRead);
        }

        usleep(sleepDurationUs);
    }

    return static_cast<int>(nbBytesRead);
#endif
}

// =============================================================================
// Special operations
// =============================================================================

bool SerialPort::flushReceiver() {
#if defined(_WIN32) || defined(_WIN64)
    return PurgeComm(handle_, PURGE_RXCLEAR) != 0;
#endif
#if defined(__linux__) || defined(__APPLE__)
    tcflush(fd_, TCIFLUSH);
    return true;
#endif
}

int SerialPort::available() {
#if defined(_WIN32) || defined(_WIN64)
    DWORD commErrors;
    COMSTAT commStatus;
    ClearCommError(handle_, &commErrors, &commStatus);
    return static_cast<int>(commStatus.cbInQue);
#endif
#if defined(__linux__) || defined(__APPLE__)
    int nBytes = 0;
    ioctl(fd_, FIONREAD, &nBytes);
    return nBytes;
#endif
}

// =============================================================================
// I/O control — DTR/RTS
// =============================================================================

bool SerialPort::setDtr(bool status) {
    if (status)
        return setDtr();
    else
        return clearDtr();
}

bool SerialPort::setDtr() {
#if defined(_WIN32) || defined(_WIN64)
    currentStateDtr_ = true;
    return EscapeCommFunction(handle_, SETDTR) != 0;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int status = 0;
    ioctl(fd_, TIOCMGET, &status);
    status |= TIOCM_DTR;
    ioctl(fd_, TIOCMSET, &status);
    return true;
#endif
}

bool SerialPort::clearDtr() {
#if defined(_WIN32) || defined(_WIN64)
    currentStateDtr_ = false;
    return EscapeCommFunction(handle_, CLRDTR) != 0;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int status = 0;
    ioctl(fd_, TIOCMGET, &status);
    status &= ~TIOCM_DTR;
    ioctl(fd_, TIOCMSET, &status);
    return true;
#endif
}

bool SerialPort::setRts(bool status) {
    if (status)
        return setRts();
    else
        return clearRts();
}

bool SerialPort::setRts() {
#if defined(_WIN32) || defined(_WIN64)
    currentStateRts_ = true;
    return EscapeCommFunction(handle_, SETRTS) != 0;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int status = 0;
    ioctl(fd_, TIOCMGET, &status);
    status |= TIOCM_RTS;
    ioctl(fd_, TIOCMSET, &status);
    return true;
#endif
}

bool SerialPort::clearRts() {
#if defined(_WIN32) || defined(_WIN64)
    currentStateRts_ = false;
    return EscapeCommFunction(handle_, CLRRTS) != 0;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int status = 0;
    ioctl(fd_, TIOCMGET, &status);
    status &= ~TIOCM_RTS;
    ioctl(fd_, TIOCMSET, &status);
    return true;
#endif
}

// =============================================================================
// Modem line status queries
// =============================================================================

bool SerialPort::isCts() {
#if defined(_WIN32) || defined(_WIN64)
    DWORD modemStat;
    GetCommModemStatus(handle_, &modemStat);
    return (modemStat & MS_CTS_ON) != 0;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int status = 0;
    ioctl(fd_, TIOCMGET, &status);
    return (status & TIOCM_CTS) != 0;
#endif
}

bool SerialPort::isDsr() {
#if defined(_WIN32) || defined(_WIN64)
    DWORD modemStat;
    GetCommModemStatus(handle_, &modemStat);
    return (modemStat & MS_DSR_ON) != 0;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int status = 0;
    ioctl(fd_, TIOCMGET, &status);
    return (status & TIOCM_DSR) != 0;
#endif
}

bool SerialPort::isDcd() {
#if defined(_WIN32) || defined(_WIN64)
    DWORD modemStat;
    GetCommModemStatus(handle_, &modemStat);
    return (modemStat & MS_RLSD_ON) != 0;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int status = 0;
    ioctl(fd_, TIOCMGET, &status);
    return (status & TIOCM_CAR) != 0;
#endif
}

bool SerialPort::isRi() {
#if defined(_WIN32) || defined(_WIN64)
    DWORD modemStat;
    GetCommModemStatus(handle_, &modemStat);
    return (modemStat & MS_RING_ON) != 0;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int status = 0;
    ioctl(fd_, TIOCMGET, &status);
    return (status & TIOCM_RNG) != 0;
#endif
}

bool SerialPort::isDtr() {
#if defined(_WIN32) || defined(_WIN64)
    return currentStateDtr_;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int status = 0;
    ioctl(fd_, TIOCMGET, &status);
    return (status & TIOCM_DTR) != 0;
#endif
}

bool SerialPort::isRts() {
#if defined(_WIN32) || defined(_WIN64)
    return currentStateRts_;
#endif
#if defined(__linux__) || defined(__APPLE__)
    int status = 0;
    ioctl(fd_, TIOCMGET, &status);
    return (status & TIOCM_RTS) != 0;
#endif
}

// =============================================================================
// Private helpers
// =============================================================================

int SerialPort::readStringNoTimeout(char* str, char finalChar,
                                    unsigned int maxBytes) {
    unsigned int nbBytes = 0;
    int charRead;

    while (nbBytes < maxBytes) {
        charRead = readChar(&str[nbBytes]);

        if (charRead == 1) {
            if (str[nbBytes] == finalChar) {
                str[++nbBytes] = 0;
                return static_cast<int>(nbBytes);
            }
            nbBytes++;
        }

        if (charRead < 0)
            return charRead;
    }

    return -3;
}

} // namespace core::serial