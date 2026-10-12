#include "Falcon/EmbeddedServer.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ILogEndPoint.h"
#include "Level/WorldManager.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Server/ServerHost.h"
#include "Server/ServerPaths.h"

#include <chrono>
#include <stdexcept>

namespace {
    const std::chrono::milliseconds TICK_INTERVAL(50);
    const std::chrono::seconds CATCHUP_RESET_INTERVAL(1);
    class CallbackLogEndPoint : public ILogEndPoint {
    public:
        explicit CallbackLogEndPoint(std::function<void(LogLevel, const std::string &)> callback)
                : mCallback(std::move(callback)) {}
        void log(const LogDetails &details) override {
            // Host callbacks must not unwind through the logger's worker.
            try { mCallback(details.mLevel, details.mMessage); } catch (...) {}
        }
    private:
        std::function<void(LogLevel, const std::string &)> mCallback;
    };
}

EmbeddedServer::EmbeddedServer() = default;
EmbeddedServer::~EmbeddedServer() { stop(); }

bool EmbeddedServer::start(const EmbeddedServerConfig &config) { return startAsync(config).get(); }

std::future<bool> EmbeddedServer::startAsync(const EmbeddedServerConfig &config) {
    auto started = std::make_shared<std::promise<bool>>();
    auto result = started->get_future();
    std::lock_guard<std::mutex> lock(mMutex);
    if (mState == State::Starting || mState == State::Running || mState == State::Stopping) {
        started->set_value(false);
        return result;
    }
    if (mThread.joinable()) mThread.join();
    if (mLogEndPoint) {
        BedrockLog::removeEndPoint(mLogEndPoint);
        mLogEndPoint.reset();
    }
    mError = {};
    mStopRequested.store(false);
    mPaused.store(false);
    mState = State::Starting;
    mProgress = Progress::Validating;
    try {
        if (config.log) {
            mLogEndPoint = std::make_shared<CallbackLogEndPoint>(config.log);
            BedrockLog::addEndPoint(mLogEndPoint);
        }
        mThread = std::thread(&EmbeddedServer::_run, this, config, started);
    } catch (const std::exception &error) {
        mError = {ErrorCode::Startup, error.what()};
        mState = State::Failed;
        mProgress = Progress::None;
        started->set_value(false);
    } catch (...) {
        mError = {ErrorCode::Startup, "Could not create server thread"};
        mState = State::Failed;
        mProgress = Progress::None;
        started->set_value(false);
    }
    return result;
}

void EmbeddedServer::requestStop() {
    mStopRequested.store(true);
    std::lock_guard<std::mutex> lock(mMutex);
    if (mState == State::Running) {
        mState = State::Stopping;
        mProgress = Progress::Stopping;
    }
}

void EmbeddedServer::stop() {
    requestStop();
    if (mThread.joinable()) mThread.join();
    if (mLogEndPoint) {
        BedrockLog::flush();
        BedrockLog::removeEndPoint(mLogEndPoint);
        mLogEndPoint.reset();
    }
}

void EmbeddedServer::setPaused(bool paused) { mPaused.store(paused); }
bool EmbeddedServer::isPaused() const { return mPaused.load(); }
bool EmbeddedServer::isRunning() const { return getState() == State::Running; }
unsigned short EmbeddedServer::getPort() const { return mPort.load(); }
EmbeddedServer::State EmbeddedServer::getState() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mState;
}
EmbeddedServer::Error EmbeddedServer::getError() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mError;
}

EmbeddedServer::Progress EmbeddedServer::getProgress() const {
    std::lock_guard<std::mutex> lock(mMutex);
    return mProgress;
}

void EmbeddedServer::_enqueue(std::function<void(ServerNetworkHandler &)> task) {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mState != State::Running || mStopRequested.load())
        throw std::runtime_error("Embedded server is not running");
    mTasks.push_back(std::move(task));
}

std::future<void> EmbeddedServer::executeCommand(std::string command) {
    auto promise = std::make_shared<std::promise<void>>();
    auto result = promise->get_future();
    try {
        _enqueue([promise, command = std::move(command)](ServerNetworkHandler &handler) {
            try { handler.executeConsoleCommand(command); promise->set_value(); }
            catch (...) { promise->set_exception(std::current_exception()); }
        });
    } catch (...) { promise->set_exception(std::current_exception()); }
    return result;
}

std::future<void> EmbeddedServer::save() {
    auto promise = std::make_shared<std::promise<void>>();
    auto result = promise->get_future();
    try {
        _enqueue([promise](ServerNetworkHandler &handler) {
            try { handler.autoSave(); promise->set_value(); }
            catch (...) { promise->set_exception(std::current_exception()); }
        });
    } catch (...) { promise->set_exception(std::current_exception()); }
    return result;
}

std::future<std::vector<std::string>> EmbeddedServer::getPlayerNames() {
    auto promise = std::make_shared<std::promise<std::vector<std::string>>>();
    auto result = promise->get_future();
    try {
        _enqueue([promise](ServerNetworkHandler &handler) {
            try {
                std::vector<std::string> names;
                for (const auto &entry: handler.getPlayers())
                    if (!entry.second.getName().empty()) names.push_back(entry.second.getName());
                promise->set_value(std::move(names));
            } catch (...) { promise->set_exception(std::current_exception()); }
        });
    } catch (...) { promise->set_exception(std::current_exception()); }
    return result;
}

void EmbeddedServer::_run(EmbeddedServerConfig config, std::shared_ptr<std::promise<bool>> started) noexcept {
    bool reported = false;
    bool rootChanged = false;
    std::filesystem::path previousRoot;
    ServerHost host;
    ErrorCode phase = ErrorCode::InvalidConfiguration;
    try {
        if (config.dataDirectory.empty() || !WorldManager::isValidName(config.levelName))
            throw std::invalid_argument("A data directory and a valid world name are required");
        for (const auto &property: config.properties)
            if (property.first.empty() || property.first.find_first_of("\r\n=") != std::string::npos
                || property.second.find_first_of("\r\n") != std::string::npos)
                throw std::invalid_argument("Invalid property key or value");
        config.dataDirectory = std::filesystem::absolute(config.dataDirectory);
        if (!host.acquire()) {
            phase = ErrorCode::Busy;
            throw std::runtime_error("Another Falcon server already owns this process");
        }
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mProgress = Progress::PreparingData;
        }
        std::filesystem::create_directories(config.dataDirectory);
        previousRoot = ServerPaths::getRoot();
        ServerPaths::setRoot(config.dataDirectory);
        rootChanged = true;
        phase = ErrorCode::Startup;
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
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mProgress = Progress::LoadingWorld;
        }
        if (!host.start(options))
            throw std::runtime_error("Could not start embedded server; see server log");
        {
            std::lock_guard<std::mutex> lock(mMutex);
            mState = mStopRequested.load() ? State::Stopping : State::Running;
            mProgress = mStopRequested.load() ? Progress::Stopping : Progress::Listening;
            mPort.store(host.getPort());
        }
        started->set_value(true);
        reported = true;
        phase = ErrorCode::Runtime;
        auto nextTick = std::chrono::steady_clock::now();
        while (!mStopRequested.load() && !host.isStopRequested()) {
            std::vector<std::function<void(ServerNetworkHandler &)>> tasks;
            {
                std::lock_guard<std::mutex> lock(mMutex);
                tasks.swap(mTasks);
            }
            for (auto &task: tasks) task(host.getHandler());
            host.tick(mPaused.load());
            nextTick += TICK_INTERVAL;
            const auto now = std::chrono::steady_clock::now();
            if (now - nextTick > CATCHUP_RESET_INTERVAL) nextTick = now;
            else std::this_thread::sleep_until(nextTick);
        }
    } catch (const std::exception &error) {
        std::lock_guard<std::mutex> lock(mMutex);
        mError = {phase == ErrorCode::Startup && dynamic_cast<const std::invalid_argument *>(&error)
                  ? ErrorCode::InvalidConfiguration : phase, error.what()};
    } catch (...) {
        std::lock_guard<std::mutex> lock(mMutex);
        mError = {phase, "Unknown embedded server exception"};
    }
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mState = State::Stopping;
        mProgress = Progress::Stopping;
        mTasks.clear(); // Abandoned futures receive broken_promise.
    }
    try { host.stop(); }
    catch (const std::exception &error) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mError.code == ErrorCode::None) mError = {ErrorCode::Shutdown, error.what()};
    } catch (...) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (mError.code == ErrorCode::None) mError = {ErrorCode::Shutdown, "Unknown shutdown exception"};
    }
    if (rootChanged) ServerPaths::setRoot(previousRoot);
    host.release();
    if (mLogEndPoint) {
        BedrockLog::flush();
        BedrockLog::removeEndPoint(mLogEndPoint);
        mLogEndPoint.reset();
    }
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mPort.store(0);
        mState = mError.code == ErrorCode::None ? State::Stopped : State::Failed;
        mProgress = Progress::None;
    }
    if (!reported) started->set_value(false);
}
