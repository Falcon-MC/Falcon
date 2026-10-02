#include "Level/AutoCompaction.h"

#include "Core/Debug/BedrockLog.h"
#include "Level/Level.h"

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
            if (gSignal.wait_for(lock, std::chrono::seconds(intervalSeconds),
                                 []() { return !gRunning.load(); }))
                return;

            // The lock only guards the wait; it is released during compaction so stop() can
            // still signal without blocking behind a long compaction.
            lock.unlock();
            {
                // Held for the whole pass so untrack() cannot return while a level it removes is compacting.
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

    // Taking the mutex once after clearing gRunning guarantees the worker is either before
    // its predicate check or already waiting, so the notification cannot be lost.
    {
        std::lock_guard<std::mutex> lock(gMutex);
    }
    gSignal.notify_all();

    if (gThread.joinable())
        gThread.join();
}
