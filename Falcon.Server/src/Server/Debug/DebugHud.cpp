#include "Server/Debug/DebugHud.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/ChunkWorker.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#else
#include <unistd.h>
#endif

namespace {
    const uint32_t REFRESH_TICKS = 20;
    const double TICKS_PER_SECOND = 20.0;
    const double BYTES_PER_MEGABYTE = 1024.0 * 1024.0;
    const float WARNING_LOAD = 0.5f;
    const float CRITICAL_LOAD = 0.8f;

    uint64_t residentBytes() {
#if defined(_WIN32)
        PROCESS_MEMORY_COUNTERS counters;
        // kernel32 entry point: no psapi import library needed
        if (K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
            return (uint64_t) counters.WorkingSetSize;
        return 0;
#elif defined(__APPLE__)
        mach_task_basic_info info;
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t) &info, &count) == KERN_SUCCESS)
            return (uint64_t) info.resident_size;
        return 0;
#else
        std::ifstream statm("/proc/self/statm");
        uint64_t totalPages = 0;
        uint64_t residentPages = 0;
        statm >> totalPages >> residentPages;
        return residentPages * (uint64_t) sysconf(_SC_PAGESIZE);
#endif
    }

    PlayerBossBars::Color colorForLoad(float load) {
        if (load >= CRITICAL_LOAD)
            return PlayerBossBars::Color::Red;
        return load >= WARNING_LOAD ? PlayerBossBars::Color::Yellow : PlayerBossBars::Color::Green;
    }

    std::string format(const char *pattern, double first, double second, double third) {
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), pattern, first, second, third);
        return buffer;
    }
}

void DebugHud::setPage(ServerPlayer &player, Page page) {
    mPage = page;
    mTicksUntilRefresh = 0;
    if (page == Page::Off && mBar != 0) {
        player.getBossBars().remove(mBar);
        mBar = 0;
    }
}

void DebugHud::tick(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (mPage == Page::Off)
        return;

    // a deopped player stops reading the server state
    if (!player.isOp()) {
        setPage(player, Page::Off);
        return;
    }

    if (mTicksUntilRefresh > 0) {
        --mTicksUntilRefresh;
        return;
    }
    mTicksUntilRefresh = REFRESH_TICKS;

    const double millisecondsPerTick = owner.getMillisecondsPerTick();
    const double tickBudget = 1000.0 / TICKS_PER_SECOND;
    std::string title;
    float load = 0.0f;
    switch (mPage) {
        case Page::Performance: {
            const double ticksPerSecond = millisecondsPerTick <= 0.0
                                                  ? TICKS_PER_SECOND
                                                  : std::min(TICKS_PER_SECOND, 1000.0 / millisecondsPerTick);
            title = format("TPS %.2f  MSPT %.2f  peak %.2f", ticksPerSecond, millisecondsPerTick,
                           owner.getPeakMillisecondsPerTick());
            load = (float) (millisecondsPerTick / tickBudget);
            break;
        }
        case Page::Generation: {
            const Level &level = owner.getLevelFor(player);
            const ChunkWorker *worker = level.getChunkWorker();
            const size_t pending = worker == nullptr ? 0 : worker->getPendingTaskCount();
            const uint64_t generated = worker == nullptr ? 0 : worker->getGeneratedCount();
            title = format("Chunks %.0f  pending %.0f  generated %.0f", (double) level.getLoadedChunkCount(),
                           (double) pending, (double) generated);
            const size_t threads = worker == nullptr ? 1 : std::max<size_t>(1, worker->getThreadCount());
            load = std::min(1.0f, (float) pending / (float) (threads * 64));
            break;
        }
        case Page::Network: {
            size_t online = 0;
            for (const auto &entry: owner.getPlayers()) {
                if (entry.second.isSpawned())
                    ++online;
            }
            title = format("Players %.0f/%.0f  chunks sent to you %.0f", (double) online,
                           (double) owner.getMaxPlayers(), (double) player.getSentChunkCount());
            load = (float) online / (float) std::max(1, (int) owner.getMaxPlayers());
            break;
        }
        case Page::Memory: {
            const double megabytes = (double) residentBytes() / BYTES_PER_MEGABYTE;
            title = format("Memory %.0f MB  chunks %.0f", megabytes,
                           (double) owner.getLevelFor(player).getLoadedChunkCount(), 0.0);
            load = 0.0f;
            break;
        }
        case Page::Off:
            return;
    }

    load = std::min(1.0f, std::max(0.0f, load));
    PlayerBossBars &bars = player.getBossBars();
    if (mBar == 0)
        mBar = bars.add(title, load, colorForLoad(load));
    else
        bars.update(mBar, title, load, colorForLoad(load));
}
