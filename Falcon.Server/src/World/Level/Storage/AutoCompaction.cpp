#include "World/Level/Storage/AutoCompaction.h"

#include "Core/Debug/BedrockLog.h"
#include "World/Level/Level.h"
#include "World/Level/Storage/LevelStorage.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
    std::thread gThread;
    std::mutex gMutex;
    std::condition_variable gSignal;
    std::atomic<bool> gRunning{false};
    std::mutex gLevelsMutex;
    std::vector<Level *> gLevels;

    // a whole-database compaction holds back the ones keeping level 0 small: any steady writing (terrain generated,
    // chunks saved) ends with every writer, the tick's saves included, waiting behind it, minutes on a large world
    const uint64_t QUIET_WRITES_PER_CHECK = 10;
    const int QUIET_CHECK_SECONDS = 10;
    const int QUIET_CHECKS = 6;

    // true once stopped
    bool waitOrStop(std::unique_lock<std::mutex> &lock, int seconds) {
        return gSignal.wait_for(lock, std::chrono::seconds(seconds), []() { return !gRunning.load(); });
    }

    void run(int intervalSeconds) {
        std::unique_lock<std::mutex> lock(gMutex);
        if (waitOrStop(lock, intervalSeconds))
            return;

        bool postponed = false;
        int quietChecks = 0;
        while (true) {
            const uint64_t writesBefore = LevelStorage::getWriteCount();
            if (waitOrStop(lock, QUIET_CHECK_SECONDS))
                return;
            if (LevelStorage::getWriteCount() - writesBefore > QUIET_WRITES_PER_CHECK) {
                if (!postponed)
                    LOG_INFO(LogAreaID::Server, "AutoCompaction postponed while the world is being written");
                postponed = true;
                quietChecks = 0;
                continue;
            }
            if (++quietChecks < QUIET_CHECKS)
                continue;
            postponed = false;
            quietChecks = 0;

            std::vector<std::pair<std::string, std::function<void()>>> tasks;
            {
                std::lock_guard<std::mutex> levels(gLevelsMutex);
                for (Level *level: gLevels) {
                    if (level->isStorageOpen())
                        tasks.emplace_back(level->getName(), level->storageCompactionTask());
                }
            }

            // released while compacting: stop() can signal without blocking behind a long compaction
            lock.unlock();
            for (const auto &task: tasks) {
                if (!gRunning.load())
                    return;
                if (!task.second)
                    continue;

                LOG_INFO(LogAreaID::Server, "Running AutoCompaction on %s...", task.first.c_str());
                task.second();
            }
            tasks.clear();
            lock.lock();

            if (waitOrStop(lock, intervalSeconds))
                return;
        }
    }
}

void AutoCompaction::start(int intervalSeconds) {
    if (intervalSeconds <= 0 || gRunning.load())
        return;

    // a thread stopped with requestStop() may still be finishing its compaction
    if (gThread.joinable())
        gThread.join();
    gRunning.store(true);
    gThread = std::thread(run, intervalSeconds);
}

void AutoCompaction::track(Level &level) {
    std::lock_guard<std::mutex> levels(gLevelsMutex);
    if (std::find(gLevels.begin(), gLevels.end(), &level) == gLevels.end())
        gLevels.push_back(&level);
}

void AutoCompaction::untrack(Level &level) {
    std::lock_guard<std::mutex> levels(gLevelsMutex);
    gLevels.erase(std::remove(gLevels.begin(), gLevels.end(), &level), gLevels.end());
}

void AutoCompaction::requestStop() {
    if (!gRunning.exchange(false))
        return;

    // lock once after clearing gRunning: the worker is before its predicate check or already waiting,
    // the notification cannot be lost
    {
        std::lock_guard<std::mutex> lock(gMutex);
    }
    gSignal.notify_all();
}

void AutoCompaction::stop() {
    requestStop();
    if (gThread.joinable())
        gThread.join();
}
