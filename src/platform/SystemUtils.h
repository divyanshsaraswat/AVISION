#pragma once

#include <cstdlib>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
#elif defined(__linux__)
#include <unistd.h>
#include <ios>
#include <iostream>
#include <fstream>
#include <string>
#endif

namespace SystemUtils {

    inline long long getMemoryUsage() {
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            return pmc.PrivateUsage; // Bytes
        }
        return 0;
#elif defined(__linux__)
        long long rss = 0;
        std::ifstream stat_stream("/proc/self/statm", std::ios_base::in);
        if (stat_stream.is_open()) {
            long program_size, resident, share, text, lib, data, dt;
            stat_stream >> program_size >> resident >> share >> text >> lib >> data >> dt;
            // Resident Set Size (pages) * Page Size
            long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
            rss = resident * page_size_kb * 1024; 
        }
        return rss;
#else
        return 0;
#endif
    }

}
