#include "Plugin/PluginScheduler.h"

#include "Core/Debug/BedrockLog.h"
#include "Plugin/LoadedPlugin.h"

#include <algorithm>
#include <utility>

namespace {
    void runGuarded(const LoadedPlugin &plugin, FalconTask task, void *userData) {
        if (task == nullptr)
            return;

        try {
            task(userData);
        } catch (...) {
            LOG_ERROR(LogAreaID::Server, "[%s] A task threw an exception", plugin.mDescription.mName.c_str());
        }
    }
}

PluginScheduler::PluginScheduler() {
    mWorker = std::thread([this] {
        _workerLoop();
    });
}

PluginScheduler::~PluginScheduler() {
    shutdown();
}

uint64_t PluginScheduler::schedule(LoadedPlugin &plugin, FalconTask task, void *userData, uint64_t delayTicks,
                                   uint64_t periodTicks) {
    const uint64_t id = mNextId++;
    mTasks.push_back(Task{id, &plugin, task, userData, mCurrentTick + std::max<uint64_t>(delayTicks, 1),
                          periodTicks, false});
    return id;
}

uint64_t PluginScheduler::runAsync(LoadedPlugin &plugin, FalconTask work, FalconTask done, void *userData) {
    const uint64_t id = mNextId++;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mPending.push_back(AsyncJob{id, &plugin, work, done, userData});
    }
    mCondition.notify_one();
    return id;
}

void PluginScheduler::cancel(uint64_t id) {
    for (Task &task: mTasks) {
        if (task.mId == id)
            task.mCancelled = true;
    }
}

void PluginScheduler::cancelAll(const LoadedPlugin &plugin) {
    for (Task &task: mTasks) {
        if (task.mPlugin == &plugin)
            task.mCancelled = true;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    for (auto it = mPending.begin(); it != mPending.end();) {
        if (it->mPlugin != &plugin) {
            ++it;
            continue;
        }

        mCompleted.push_back(*it);
        it = mPending.erase(it);
    }
}

void PluginScheduler::tick() {
    mCurrentTick++;

    const size_t count = mTasks.size();
    for (size_t index = 0; index < count; index++) {
        Task task = mTasks[index];
        if (task.mCancelled || task.mNextTick > mCurrentTick || !task.mPlugin->mEnabled)
            continue;

        runGuarded(*task.mPlugin, task.mTask, task.mUserData);

        if (task.mPeriod == 0)
            mTasks[index].mCancelled = true;
        else
            mTasks[index].mNextTick = mCurrentTick + task.mPeriod;
    }

    mTasks.erase(std::remove_if(mTasks.begin(), mTasks.end(), [](const Task &task) {
        return task.mCancelled;
    }), mTasks.end());

    std::deque<AsyncJob> completed;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        completed.swap(mCompleted);
    }

    for (const AsyncJob &job: completed)
        runGuarded(*job.mPlugin, job.mDone, job.mUserData);
}

void PluginScheduler::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mStopping)
            return;
        mStopping = true;
    }
    mCondition.notify_all();

    if (mWorker.joinable())
        mWorker.join();

    mTasks.clear();

    std::deque<AsyncJob> remaining;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        remaining.swap(mCompleted);
        remaining.insert(remaining.end(), mPending.begin(), mPending.end());
        mPending.clear();
    }

    for (const AsyncJob &job: remaining)
        runGuarded(*job.mPlugin, job.mDone, job.mUserData);
}

void PluginScheduler::_workerLoop() {
    for (;;) {
        AsyncJob job;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            mCondition.wait(lock, [this] {
                return mStopping || !mPending.empty();
            });

            if (mStopping)
                return;

            job = mPending.front();
            mPending.pop_front();
        }

        runGuarded(*job.mPlugin, job.mWork, job.mUserData);

        std::lock_guard<std::mutex> lock(mMutex);
        mCompleted.push_back(job);
    }
}
