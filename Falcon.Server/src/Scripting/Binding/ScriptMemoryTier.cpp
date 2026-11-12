#include "Scripting/Binding/ScriptMemoryTier.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#else
#include <unistd.h>
#endif

namespace ScriptMemoryTier {
    uint64_t physicalMemory() {
#if defined(_WIN32)
        MEMORYSTATUSEX status;
        status.dwLength = sizeof(status);
        // the game reads ullTotalPhys the same way (sub_140083100, 26.60)
        return GlobalMemoryStatusEx(&status) ? (uint64_t) status.ullTotalPhys : 0;
#elif defined(__APPLE__)
        uint64_t bytes = 0;
        size_t length = sizeof(bytes);
        return sysctlbyname("hw.memsize", &bytes, &length, nullptr, 0) == 0 ? bytes : 0;
#else
        const long pages = sysconf(_SC_PHYS_PAGES);
        const long pageSize = sysconf(_SC_PAGE_SIZE);
        return pages > 0 && pageSize > 0 ? (uint64_t) pages * (uint64_t) pageSize : 0;
#endif
    }

    // SuperLow up to 1.5 GB, Low 2, Mid 4, High 8, SuperHigh above
    int32_t tierOf(uint64_t bytes) {
        const uint64_t gigabyte = 1024ull * 1024ull * 1024ull;
        if (bytes <= gigabyte * 3 / 2)
            return 0;
        if (bytes <= gigabyte * 2)
            return 1;
        if (bytes <= gigabyte * 4)
            return 2;
        if (bytes <= gigabyte * 8)
            return 3;
        return 4;
    }
}
