#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <iterator>
#include <mutex>
#include <utility>
#include <vector>

template<typename T>
class TaskQueue {
public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> guard(mMutex);
            if (mClosed)
                return;

            mItems.push_back(std::move(value));
        }

        mCondition.notify_one();
    }

    bool waitPop(T &out) {
        std::unique_lock<std::mutex> lock(mMutex);
        mCondition.wait(lock, [this]() { return mClosed || !mItems.empty(); });

        if (mItems.empty())
            return false;

        out = std::move(mItems.front());
        mItems.pop_front();
        return true;
    }

    std::vector<T> drain() {
        std::lock_guard<std::mutex> guard(mMutex);

        std::vector<T> result(std::make_move_iterator(mItems.begin()), std::make_move_iterator(mItems.end()));
        mItems.clear();
        return result;
    }

    size_t size() const {
        std::lock_guard<std::mutex> guard(mMutex);
        return mItems.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> guard(mMutex);
        return mItems.empty();
    }

    void close() {
        {
            std::lock_guard<std::mutex> guard(mMutex);
            mClosed = true;
        }

        mCondition.notify_all();
    }

private:
    mutable std::mutex mMutex;
    std::condition_variable mCondition;
    std::deque<T> mItems;
    bool mClosed = false;
};
