#include "Core/Debug/BedrockLog.h"

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace BedrockLog {

    namespace {

        std::mutex &getEndPointMutex() {
            static std::mutex mutex;
            return mutex;
        }

        std::vector<std::shared_ptr<ILogEndPoint> > &getEndPoints() {
            static std::vector<std::shared_ptr<ILogEndPoint> > endPoints;
            return endPoints;
        }

        LogLevel &getMinimumLevel() {
            static LogLevel level = LogLevel::Info;
            return level;
        }

        class AsyncLogWriter {
        public:
            static AsyncLogWriter &getInstance() {
                static AsyncLogWriter instance;
                return instance;
            }

            ~AsyncLogWriter() {
                stop();
            }

            void push(LogDetails details) {
                {
                    std::lock_guard<std::mutex> guard(mMutex);

                    if (mStopping) {
                        _dispatchOne(details);
                        _flushEndPoints();
                        return;
                    }

                    _startLocked();
                    mPending.push_back(std::move(details));
                }

                mWakeCondition.notify_one();
            }

            void waitUntilDrained() {
                std::unique_lock<std::mutex> lock(mMutex);
                mDoneCondition.wait(lock, [this]() { return mPending.empty() && !mBusy; });
            }

            void stop() {
                {
                    std::lock_guard<std::mutex> guard(mMutex);
                    mStopping = true;

                    if (!mRunning)
                        return;
                }

                mWakeCondition.notify_all();

                if (mThread.joinable())
                    mThread.join();

                std::lock_guard<std::mutex> guard(mMutex);
                mRunning = false;
            }

        private:
            AsyncLogWriter() = default;

            void _startLocked() {
                if (mRunning)
                    return;

                mRunning = true;
                mThread = std::thread(&AsyncLogWriter::_run, this);
            }

            static void _dispatchOne(const LogDetails &details) {
                std::vector<std::shared_ptr<ILogEndPoint> > endPoints;

                {
                    std::lock_guard<std::mutex> guard(getEndPointMutex());
                    endPoints = getEndPoints();
                }

                for (const std::shared_ptr<ILogEndPoint> &endPoint: endPoints)
                    endPoint->log(details);
            }

            static void _flushEndPoints() {
                std::vector<std::shared_ptr<ILogEndPoint> > endPoints;

                {
                    std::lock_guard<std::mutex> guard(getEndPointMutex());
                    endPoints = getEndPoints();
                }

                for (const std::shared_ptr<ILogEndPoint> &endPoint: endPoints)
                    endPoint->flush();
            }

            void _run() {
                std::deque<LogDetails> batch;

                while (true) {
                    {
                        std::unique_lock<std::mutex> lock(mMutex);
                        mWakeCondition.wait(lock, [this]() { return mStopping || !mPending.empty(); });

                        if (mPending.empty() && mStopping)
                            return;

                        batch.swap(mPending);
                        mBusy = true;
                    }

                    std::vector<std::shared_ptr<ILogEndPoint> > endPoints;

                    {
                        std::lock_guard<std::mutex> guard(getEndPointMutex());
                        endPoints = getEndPoints();
                    }

                    for (const LogDetails &details: batch) {
                        for (const std::shared_ptr<ILogEndPoint> &endPoint: endPoints)
                            endPoint->log(details);
                    }

                    batch.clear();

                    bool drained;

                    {
                        std::lock_guard<std::mutex> guard(mMutex);
                        drained = mPending.empty();
                    }

                    if (drained) {
                        for (const std::shared_ptr<ILogEndPoint> &endPoint: endPoints)
                            endPoint->flush();
                    }

                    {
                        std::lock_guard<std::mutex> guard(mMutex);
                        mBusy = false;
                    }

                    mDoneCondition.notify_all();
                }
            }

            std::thread mThread;
            std::mutex mMutex;
            std::condition_variable mWakeCondition;
            std::condition_variable mDoneCondition;
            std::deque<LogDetails> mPending;
            bool mRunning = false;
            bool mStopping = false;
            bool mBusy = false;
        };

        std::string formatMessage(const char *format, va_list args) {
            va_list argsCopy;
            va_copy(argsCopy, args);

            const int length = vsnprintf(nullptr, 0, format, argsCopy);
            va_end(argsCopy);

            if (length <= 0)
                return std::string();

            std::string message((size_t) length, '\0');
            vsnprintf(&message[0], (size_t) length + 1, format, args);
            return message;
        }

    }

    void addEndPoint(const std::shared_ptr<ILogEndPoint> &endPoint) {
        if (!endPoint)
            return;

        std::lock_guard<std::mutex> guard(getEndPointMutex());

        std::vector<std::shared_ptr<ILogEndPoint> > &endPoints = getEndPoints();
        if (std::find(endPoints.begin(), endPoints.end(), endPoint) == endPoints.end())
            endPoints.push_back(endPoint);
    }

    void removeEndPoint(const std::shared_ptr<ILogEndPoint> &endPoint) {
        std::lock_guard<std::mutex> guard(getEndPointMutex());

        std::vector<std::shared_ptr<ILogEndPoint> > &endPoints = getEndPoints();
        endPoints.erase(std::remove(endPoints.begin(), endPoints.end(), endPoint), endPoints.end());
    }

    void clearEndPoints() {
        std::lock_guard<std::mutex> guard(getEndPointMutex());
        getEndPoints().clear();
    }

    void setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> guard(getEndPointMutex());
        getMinimumLevel() = level;
    }

    LogLevel getLogLevel() {
        std::lock_guard<std::mutex> guard(getEndPointMutex());
        return getMinimumLevel();
    }

    void log_va(LogCategory category, LogRule rule, LogAreaID area, LogLevel level, const char *file, int line,
                const char *format, va_list args) {
        (void) category;

        {
            std::lock_guard<std::mutex> guard(getEndPointMutex());

            if (rule != LogRule::IgnoreLevel && level < getMinimumLevel())
                return;

            if (getEndPoints().empty())
                return;
        }

        LogDetails details;
        details.mArea = area;
        details.mLevel = level;
        details.mFile = file;
        details.mLine = line;
        details.mMessage = formatMessage(format, args);

        AsyncLogWriter::getInstance().push(std::move(details));
    }

    void log(LogCategory category, LogRule rule, LogAreaID area, LogLevel level, const char *file, int line,
             const char *format, ...) {
        va_list args;
        va_start(args, format);
        log_va(category, rule, area, level, file, line, format, args);
        va_end(args);
    }

    void flush() {
        AsyncLogWriter::getInstance().waitUntilDrained();

        std::vector<std::shared_ptr<ILogEndPoint> > endPoints;

        {
            std::lock_guard<std::mutex> guard(getEndPointMutex());
            endPoints = getEndPoints();
        }

        for (const std::shared_ptr<ILogEndPoint> &endPoint: endPoints)
            endPoint->flush();
    }

    void shutdown() {
        AsyncLogWriter::getInstance().stop();
        flush();
    }

}
