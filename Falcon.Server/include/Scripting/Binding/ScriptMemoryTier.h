#pragma once

#include <cstdint>

// SystemInfo.memoryTier: the MemoryTier of the machine's physical memory
namespace ScriptMemoryTier {
    // total physical memory in bytes; 0 when the platform does not tell
    uint64_t physicalMemory();

    // the MemoryTier value for a memory size: the ceilings of the 1.26.60 declarations
    int32_t tierOf(uint64_t bytes);
}
