#include "Falcon/EmbeddedServer.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ILogEndPoint.h"
#include "Server/ServerHost.h"
#include "Server/ServerPaths.h"

#include <chrono>

namespace {
    const std::chrono::milliseconds TICK_INTERVAL(50);
    const std::chrono::seconds CATCHUP_RESET_INTERVAL(1);

    class CallbackLogEndPoint : public ILogEndPoint {
    public:
        explicit CallbackLogEndPoint(std::function<void(LogLevel, const std::string &)> callback)
                : mCallback(std::move(callback)) {}

        void log(const LogDetails &details) override {
            mCallback(details.mLevel, details.mMessage);
        }

    private:
        std::function<void(LogLevel, const std::string &)> mCallback;
    };
}

EmbeddedServer::EmbeddedServer() = default;

EmbeddedServer::~EmbeddedServer() {
    stop();
}

bool EmbeddedServer::start(const EmbeddedServerConfig &config) {
    std::unique_lock<std::mutex> lock(mMutex);
    if (mState == State::Starting || mState == State::Running)
        return false;

    if (mThread.joinable())
        mThread.join();

    if (config.log) {
        mLogEndPoint = std::make_shared<CallbackLogEndPoint>(config.log);
        BedrockLog::addEndPoint(mLogEndPoint);
    }

    mStopRequested.store(false);
    mPaused.store(false);
    mState = State::Starting;
    mThread = std::thread(&EmbeddedServer::_run, this, config);

    mStateChanged.wait(lock, [this]() {
        return mState != State::Starting;
    });

    if (mState == State::Running)
        return true;

    lock.unlock();
    mThread.join();
    if (mLogEndPoint != nullptr) {
        BedrockLog::removeEndPoint(mLogEndPoint);
        mLogEndPoint.reset();
    }
    return false;
}

void EmbeddedServer::stop() {
    mStopRequested.store(true);
    if (mThread.joinable())
        mThread.join();

    if (mLogEndPoint != nullptr) {
        BedrockLog::flush();
        BedrockLog::removeEndPoint(mLogEndPoint);
        mLogEndPoint.reset();
    }
}

void EmbeddedServer::setPaused(bool paused) {
    mPaused.store(paused);
}

bool EmbeddedServer::isPaused() const {
    return mPaused.load();
}

bool EmbeddedServer::isRunning() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mState == State::Running;
}

unsigned short EmbeddedServer::getPort() const {
    return mPort.load();
}

// The whole server lives on this thread: the game state belongs to whichever thread ticks it.
void EmbeddedServer::_run(EmbeddedServerConfig config) {
    const std::filesystem::path previousRoot = ServerPaths::getRoot();
    ServerPaths::setRoot(config.dataDirectory);

    ServerHostOptions options;
    options.properties = config.properties;
    options.properties["level-name"] = config.levelName;
    options.properties["transport"] = "raknet";
    options.properties["enable-lan-visibility"] = "false";
    options.properties["online-mode"] = "false";
    options.properties["network-encryption"] = "false";
    options.bindAddress = "127.0.0.1";
    options.portOverride = 0;
    options.plugins = false;

    ServerHost host;
    const bool started = host.start(options);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mState = started ? State::Running : State::Failed;
        mPort.store(started ? host.getPort() : 0);
    }
    mStateChanged.notify_all();

    if (!started) {
        ServerPaths::setRoot(previousRoot);
        return;
    }

    std::chrono::steady_clock::time_point nextTick = std::chrono::steady_clock::now();
    while (!mStopRequested.load() && !host.isStopRequested()) {
        if (mPaused.load()) {
            std::this_thread::sleep_for(TICK_INTERVAL);
            nextTick = std::chrono::steady_clock::now();
            continue;
        }

        host.tick();

        nextTick += TICK_INTERVAL;
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if (now - nextTick > CATCHUP_RESET_INTERVAL)
            nextTick = now;
        else
            std::this_thread::sleep_until(nextTick);
    }

    host.stop();
    ServerPaths::setRoot(previousRoot);

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mState = State::Stopped;
        mPort.store(0);
    }
    mStateChanged.notify_all();
}
