#include "Level/AutoCompaction.h"

#include "Core/Debug/BedrockLog.h"
#include "Level/Level.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace {
    std::thread gThread;
    std::mutex gMutex;
    std::condition_variable gSignal;
    std::atomic<bool> gRunning{false};

    void run(Level *level, int intervalSeconds) {
        std::unique_lock<std::mutex> lock(gMutex);

        while (gRunning.load()) {
            if (gSignal.wait_for(lock, std::chrono::seconds(intervalSeconds),
                                 []() { return !gRunning.load(); }))
                return;

            if (!level->isStorageOpen())
                continue;

            lock.unlock();
            LOG_INFO(LogAreaID::Server, "Running AutoCompaction...");
            level->compactStorage();
            lock.lock();
        }
    }
}

void AutoCompaction::start(Level &level, int intervalSeconds) {
    if (intervalSeconds <= 0 || gRunning.exchange(true))
        return;

    gThread = std::thread(run, &level, intervalSeconds);
}

void AutoCompaction::stop() {
    if (!gRunning.exchange(false))
        return;

    {
        std::lock_guard<std::mutex> lock(gMutex);
    }
    gSignal.notify_all();

    if (gThread.joinable())
        gThread.join();
}
