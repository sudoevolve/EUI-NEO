#include "core/platform/performance_stats.h"

#include <algorithm>
#include <ctime>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <cwchar>
#include <cwctype>
#include <memory>
#include <string>
#include <vector>
#endif

namespace core::platform {

#if defined(_WIN32)

namespace {

double fileTimeToSeconds(const FILETIME& value) {
    ULARGE_INTEGER integer{};
    integer.LowPart = value.dwLowDateTime;
    integer.HighPart = value.dwHighDateTime;
    return static_cast<double>(integer.QuadPart) / 10000000.0;
}

bool pathContainsPid(const std::wstring& path, DWORD pid) {
    const std::wstring token = L"pid_" + std::to_wstring(pid);
    auto it = std::search(
        path.begin(),
        path.end(),
        token.begin(),
        token.end(),
        [](wchar_t left, wchar_t right) {
            return std::towlower(left) == std::towlower(right);
        });
    return it != path.end();
}

DWORD_PTR countBits(DWORD_PTR value) {
    DWORD_PTR count = 0;
    while (value != 0) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}

DWORD cpuNormalizationCount() {
    DWORD physicalCoreCount = 0;
    DWORD length = 0;
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &length) &&
        GetLastError() == ERROR_INSUFFICIENT_BUFFER &&
        length > 0) {
        std::vector<unsigned char> buffer(length);
        if (GetLogicalProcessorInformationEx(
                RelationProcessorCore,
                reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data()),
                &length)) {
            unsigned char* cursor = buffer.data();
            const unsigned char* end = buffer.data() + length;
            while (cursor < end) {
                auto* info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(cursor);
                if (info->Relationship == RelationProcessorCore) {
                    ++physicalCoreCount;
                }
                cursor += info->Size;
            }
        }
    }
    if (physicalCoreCount > 0) {
        return physicalCoreCount;
    }

    DWORD_PTR processMask = 0;
    DWORD_PTR systemMask = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask) && processMask != 0) {
        return static_cast<DWORD>(std::max<DWORD_PTR>(1, countBits(processMask)));
    }
    return std::max<DWORD>(1, GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
}

std::wstring counterPathPrefix(const std::wstring& path) {
    const std::size_t separator = path.rfind(L'\\');
    if (separator == std::wstring::npos) {
        return {};
    }
    return path.substr(0, separator);
}

} // namespace

struct ProcessUsageSampler::Impl {
    DWORD pid = GetCurrentProcessId();
    DWORD processorCount = cpuNormalizationCount();
    double lastProcessSeconds = 0.0;
    bool hasLastProcessSeconds = false;

    PDH_HQUERY cpuQuery = nullptr;
    PDH_HCOUNTER cpuCounter = nullptr;
    bool cpuInitialized = false;
    bool cpuPrimed = false;

    PDH_HQUERY gpuQuery = nullptr;
    std::vector<PDH_HCOUNTER> gpuCounters;
    bool gpuInitialized = false;

    ~Impl() {
        if (cpuQuery != nullptr) {
            PdhCloseQuery(cpuQuery);
            cpuQuery = nullptr;
        }
        if (gpuQuery != nullptr) {
            PdhCloseQuery(gpuQuery);
            gpuQuery = nullptr;
        }
    }

    void reset() {
        lastProcessSeconds = currentProcessSeconds();
        hasLastProcessSeconds = true;
        if (cpuQuery != nullptr) {
            PdhCollectQueryData(cpuQuery);
            cpuPrimed = true;
        }
        if (gpuQuery != nullptr) {
            PdhCollectQueryData(gpuQuery);
        }
    }

    double currentProcessSeconds() const {
        FILETIME createTime{};
        FILETIME exitTime{};
        FILETIME kernelTime{};
        FILETIME userTime{};
        if (!GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime)) {
            return 0.0;
        }
        return fileTimeToSeconds(kernelTime) + fileTimeToSeconds(userTime);
    }

    bool initializeCpuCounter() {
        cpuInitialized = true;

        PDH_HQUERY idQuery = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &idQuery) != ERROR_SUCCESS) {
            return false;
        }

        const wchar_t* wildcard = L"\\Process(*)\\ID Process";
        DWORD length = 0;
        PDH_STATUS status = PdhExpandWildCardPathW(nullptr, wildcard, nullptr, &length, 0);
        if (status != PDH_MORE_DATA || length == 0) {
            PdhCloseQuery(idQuery);
            return false;
        }

        std::vector<wchar_t> buffer(length);
        status = PdhExpandWildCardPathW(nullptr, wildcard, buffer.data(), &length, 0);
        if (status != ERROR_SUCCESS) {
            PdhCloseQuery(idQuery);
            return false;
        }

        struct Candidate {
            std::wstring path;
            PDH_HCOUNTER counter = nullptr;
        };
        std::vector<Candidate> candidates;
        for (const wchar_t* path = buffer.data(); path != nullptr && *path != L'\0'; path += std::wcslen(path) + 1) {
            Candidate candidate;
            candidate.path = path;
            if (PdhAddEnglishCounterW(idQuery, candidate.path.c_str(), 0, &candidate.counter) == ERROR_SUCCESS ||
                PdhAddCounterW(idQuery, candidate.path.c_str(), 0, &candidate.counter) == ERROR_SUCCESS) {
                candidates.push_back(std::move(candidate));
            }
        }

        if (candidates.empty() || PdhCollectQueryData(idQuery) != ERROR_SUCCESS) {
            PdhCloseQuery(idQuery);
            return false;
        }

        std::wstring processPrefix;
        for (const Candidate& candidate : candidates) {
            PDH_FMT_COUNTERVALUE value{};
            if (PdhGetFormattedCounterValue(candidate.counter, PDH_FMT_LONG, nullptr, &value) == ERROR_SUCCESS &&
                value.CStatus == ERROR_SUCCESS &&
                static_cast<DWORD>(value.longValue) == pid) {
                processPrefix = counterPathPrefix(candidate.path);
                break;
            }
        }
        PdhCloseQuery(idQuery);

        if (processPrefix.empty() || PdhOpenQueryW(nullptr, 0, &cpuQuery) != ERROR_SUCCESS) {
            cpuQuery = nullptr;
            return false;
        }

        const std::wstring cpuPath = processPrefix + L"\\% Processor Time";
        if (PdhAddEnglishCounterW(cpuQuery, cpuPath.c_str(), 0, &cpuCounter) != ERROR_SUCCESS &&
            PdhAddCounterW(cpuQuery, cpuPath.c_str(), 0, &cpuCounter) != ERROR_SUCCESS) {
            PdhCloseQuery(cpuQuery);
            cpuQuery = nullptr;
            cpuCounter = nullptr;
            return false;
        }

        cpuPrimed = PdhCollectQueryData(cpuQuery) == ERROR_SUCCESS;
        return cpuPrimed;
    }

    bool sampleCpuFromPdh(double& percent) {
        if (!cpuInitialized && !initializeCpuCounter()) {
            return false;
        }
        if (cpuQuery == nullptr || cpuCounter == nullptr) {
            return false;
        }
        if (PdhCollectQueryData(cpuQuery) != ERROR_SUCCESS) {
            return false;
        }
        if (!cpuPrimed) {
            cpuPrimed = true;
            return false;
        }

        PDH_FMT_COUNTERVALUE value{};
        if (PdhGetFormattedCounterValue(cpuCounter, PDH_FMT_DOUBLE, nullptr, &value) != ERROR_SUCCESS ||
            value.CStatus != ERROR_SUCCESS) {
            return false;
        }

        percent = std::clamp(value.doubleValue / static_cast<double>(std::max<DWORD>(1, processorCount)), 0.0, 100.0);
        return true;
    }

    bool sampleCpuFromProcessTimes(double elapsedSeconds, double& percent) {
        if (elapsedSeconds <= 0.0) {
            return false;
        }
        const double current = currentProcessSeconds();
        if (!hasLastProcessSeconds) {
            lastProcessSeconds = current;
            hasLastProcessSeconds = true;
            return false;
        }

        const double delta = std::max(0.0, current - lastProcessSeconds);
        lastProcessSeconds = current;
        percent = (delta / elapsedSeconds) * 100.0 / static_cast<double>(std::max<DWORD>(1, processorCount));
        percent = std::clamp(percent, 0.0, 100.0);
        return true;
    }

    bool sampleCpu(double elapsedSeconds, double& percent) {
        return sampleCpuFromPdh(percent) || sampleCpuFromProcessTimes(elapsedSeconds, percent);
    }

    void initializeGpuCounters() {
        gpuInitialized = true;
        if (PdhOpenQueryW(nullptr, 0, &gpuQuery) != ERROR_SUCCESS) {
            gpuQuery = nullptr;
            return;
        }

        const wchar_t* wildcard = L"\\GPU Engine(*)\\Utilization Percentage";
        DWORD length = 0;
        PDH_STATUS status = PdhExpandWildCardPathW(nullptr, wildcard, nullptr, &length, 0);
        if (status != PDH_MORE_DATA || length == 0) {
            return;
        }

        std::vector<wchar_t> buffer(length);
        status = PdhExpandWildCardPathW(nullptr, wildcard, buffer.data(), &length, 0);
        if (status != ERROR_SUCCESS) {
            return;
        }

        for (const wchar_t* path = buffer.data(); path != nullptr && *path != L'\0'; path += std::wcslen(path) + 1) {
            const std::wstring counterPath(path);
            if (!pathContainsPid(counterPath, pid)) {
                continue;
            }
            PDH_HCOUNTER counter = nullptr;
            if (PdhAddEnglishCounterW(gpuQuery, counterPath.c_str(), 0, &counter) == ERROR_SUCCESS ||
                PdhAddCounterW(gpuQuery, counterPath.c_str(), 0, &counter) == ERROR_SUCCESS) {
                gpuCounters.push_back(counter);
            }
        }

        if (!gpuCounters.empty()) {
            PdhCollectQueryData(gpuQuery);
        }
    }

    bool sampleGpu(double& percent) {
        if (!gpuInitialized) {
            initializeGpuCounters();
        }
        if (gpuQuery == nullptr || gpuCounters.empty()) {
            return false;
        }
        if (PdhCollectQueryData(gpuQuery) != ERROR_SUCCESS) {
            return false;
        }

        double total = 0.0;
        bool hasValue = false;
        for (PDH_HCOUNTER counter : gpuCounters) {
            PDH_FMT_COUNTERVALUE value{};
            if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS &&
                value.CStatus == ERROR_SUCCESS) {
                total += std::max(0.0, value.doubleValue);
                hasValue = true;
            }
        }
        percent = std::clamp(total, 0.0, 100.0);
        return hasValue;
    }
};

#else

struct ProcessUsageSampler::Impl {
    std::clock_t lastProcessClock = std::clock();
    bool hasLastProcessClock = false;

    void reset() {
        lastProcessClock = std::clock();
        hasLastProcessClock = true;
    }

    bool sampleCpu(double elapsedSeconds, double& percent) {
        if (elapsedSeconds <= 0.0) {
            return false;
        }
        const std::clock_t current = std::clock();
        if (!hasLastProcessClock) {
            lastProcessClock = current;
            hasLastProcessClock = true;
            return false;
        }

        const double cpuSeconds = static_cast<double>(current - lastProcessClock) / static_cast<double>(CLOCKS_PER_SEC);
        lastProcessClock = current;
        percent = std::clamp(cpuSeconds / elapsedSeconds * 100.0, 0.0, 100.0);
        return true;
    }

    bool sampleGpu(double&) { return false; }
};

#endif

ProcessUsageSampler::ProcessUsageSampler() : impl_(std::make_unique<Impl>()) {}

ProcessUsageSampler::~ProcessUsageSampler() = default;

void ProcessUsageSampler::reset() {
    impl_->reset();
}

ProcessUsageSample ProcessUsageSampler::sample(double elapsedSeconds) {
    ProcessUsageSample sample;
    sample.hasCpuPercent = impl_->sampleCpu(elapsedSeconds, sample.cpuPercent);
    sample.hasGpuPercent = impl_->sampleGpu(sample.gpuPercent);
    return sample;
}

} // namespace core::platform
