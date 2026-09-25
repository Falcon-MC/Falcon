#pragma once

#include <falcon/falcon_api.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

struct LoadedPlugin;

class PluginScheduler {
public:
    PluginScheduler();

    ~PluginScheduler();

    uint64_t schedule(LoadedPlugin &plugin, FalconTask task, void *userData, uint64_t delayTicks,
                      uint64_t periodTicks);

    uint64_t runAsync(LoadedPlugin &plugin, FalconTask work, FalconTask done, void *userData);

    void cancel(uint64_t id);

    void cancelAll(const LoadedPlugin &plugin);

    void tick();

    void shutdown();

private:
    struct Task {
        uint64_t mId;
        LoadedPlugin *mPlugin;
        FalconTask mTask;
        void *mUserData;
        uint64_t mNextTick;
        uint64_t mPeriod;
        bool mCancelled;
    };

    struct AsyncJob {
        uint64_t mId;
        LoadedPlugin *mPlugin;
        FalconTask mWork;
        FalconTask mDone;
        void *mUserData;
    };

    void _workerLoop();

    std::vector<Task> mTasks;
    uint64_t mCurrentTick = 0;
    uint64_t mNextId = 1;

    std::thread mWorker;
    std::mutex mMutex;
    std::condition_variable mCondition;
    std::deque<AsyncJob> mPending;
    std::deque<AsyncJob> mCompleted;
    bool mStopping = false;
};
