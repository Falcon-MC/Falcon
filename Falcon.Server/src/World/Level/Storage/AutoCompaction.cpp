#include "World/Level/Storage/AutoCompaction.h"

#include "Core/Debug/BedrockLog.h"
#include "World/Level/Level.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

namespace {
    std::thread gThread;
    std::mutex gMutex;
    std::condition_variable gSignal;
    std::atomic<bool> gRunning{false};
    std::mutex gLevelsMutex;
    std::vector<Level *> gLevels;

    void run(int intervalSeconds) {
        std::unique_lock<std::mutex> lock(gMutex);

        while (gRunning.load()) {
            if (gSignal.wait_for(lock, std::chrono::seconds(intervalSeconds), []() { return !gRunning.load(); }))
                return;

            // released while compacting: stop() can signal without blocking behind a long compaction
            lock.unlock();
            {
                // whole pass: untrack() cannot return while the level it removes is compacting
                std::lock_guard<std::mutex> levels(gLevelsMutex);
                for (Level *level: gLevels) {
                    if (!level->isStorageOpen())
                        continue;

                    LOG_INFO(LogAreaID::Server, "Running AutoCompaction on %s...", level->getName().c_str());
                    level->compactStorage();
                }
            }
            lock.lock();
        }
    }
}

void AutoCompaction::start(int intervalSeconds) {
    if (intervalSeconds <= 0 || gRunning.exchange(true))
        return;

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

void AutoCompaction::stop() {
    if (!gRunning.exchange(false))
        return;

    // lock once after clearing gRunning: the worker is before its predicate check or already waiting,
    // the notification cannot be lost
    {
        std::lock_guard<std::mutex> lock(gMutex);
    }
    gSignal.notify_all();

    if (gThread.joinable())
        gThread.join();
}
