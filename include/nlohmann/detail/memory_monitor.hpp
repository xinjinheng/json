#pragma once

#include <cstddef>
#include <stdexcept>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#elif defined(__linux__)
#include <unistd.h>
#include <sys/resource.h>
#elif defined(__APPLE__)
#include <unistd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

namespace nlohmann
{
namespace detail
{

/// @brief Memory monitor class to track current memory usage
/// @sa https://json.nlohmann.me/api/memory_monitor/
class memory_monitor
{
public:
    /// @brief Get current memory usage in bytes
    /// @return Current memory usage in bytes
    /// @throws std::runtime_error if memory usage cannot be retrieved
    static std::size_t get_current_memory_usage()
    {
#if defined(_WIN32) || defined(_WIN64)
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc)))
        {
            return pmc.PrivateUsage;
        }
        throw std::runtime_error("Failed to get memory usage");
#elif defined(__linux__)
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0)
        {
            return usage.ru_maxrss * 1024; // Convert from KB to bytes
        }
        throw std::runtime_error("Failed to get memory usage");
#elif defined(__APPLE__)
        struct rusage usage;
        if (getrusage(RUSAGE_SELF, &usage) == 0)
        {
            return usage.ru_maxrss * 1024; // Convert from KB to bytes
        }
        throw std::runtime_error("Failed to get memory usage");
#else
        // Unsupported platform
        return 0;
#endif
    }

    /// @brief Get total system memory in bytes
    /// @return Total system memory in bytes
    /// @throws std::runtime_error if total memory cannot be retrieved
    static std::size_t get_total_system_memory()
    {
#if defined(_WIN32) || defined(_WIN64)
        MEMORYSTATUSEX statex;
        statex.dwLength = sizeof(statex);
        if (GlobalMemoryStatusEx(&statex))
        {
            return statex.ullTotalPhys;
        }
        throw std::runtime_error("Failed to get total system memory");
#elif defined(__linux__)
        long pages = sysconf(_SC_PHYS_PAGES);
        long page_size = sysconf(_SC_PAGE_SIZE);
        if (pages != -1 && page_size != -1)
        {
            return static_cast<std::size_t>(pages) * static_cast<std::size_t>(page_size);
        }
        throw std::runtime_error("Failed to get total system memory");
#elif defined(__APPLE__)
        int mib[2] = {CTL_HW, HW_MEMSIZE};
        uint64_t memory;
        size_t size = sizeof(memory);
        if (sysctl(mib, 2, &memory, &size, nullptr, 0) == 0)
        {
            return static_cast<std::size_t>(memory);
        }
        throw std::runtime_error("Failed to get total system memory");
#else
        // Unsupported platform
        return 0;
#endif
    }
};

} // namespace detail
} // namespace nlohmann