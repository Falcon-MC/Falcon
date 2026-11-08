#pragma once

#include "Core/Debug/LogLevel.h"

#include <atomic>
#include <filesystem>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class ILogEndPoint;

struct EmbeddedServerConfig {
    // laid out like a dedicated server's directory: server.properties, worlds, player data, packs
    std::filesystem::path dataDirectory;
    // folder under dataDirectory/worlds, created if missing
    std::string levelName;
    // extra server.properties values (gamemode, difficulty, level-seed...)
    std::map<std::string, std::string> properties;
    std::function<void(LogLevel level, const std::string &message)> log;
};

// own thread, single local player: loopback only, system-picked port, no Xbox login or encryption;
// no plugins, never touches the process (console, signals, exit); one at a time
class EmbeddedServer {
public:
    enum class State { Starting, Running, Stopping, Failed, Stopped };
    enum class Progress { None, Validating, PreparingData, LoadingWorld, Listening, Stopping };
    enum class ErrorCode { None, Busy, InvalidConfiguration, Startup, Runtime, Shutdown };
    struct Error {
        ErrorCode code = ErrorCode::None;
        std::string message;
    };
    EmbeddedServer();

    ~EmbeddedServer();

    EmbeddedServer(const EmbeddedServer &) = delete;

    EmbeddedServer &operator=(const EmbeddedServer &) = delete;

    // blocks until listening or failed; false when it could not start
    bool start(const EmbeddedServerConfig &config);
    // poll the future and getState() from the UI; start()/stop() are blocking wrappers
    // serialize lifecycle calls; log callbacks run on the logger thread
    std::future<bool> startAsync(const EmbeddedServerConfig &config);
    // returns immediately; Stopped/Failed means cleanup is done
    void requestStop();
    State getState() const;
    Progress getProgress() const;
    Error getError() const;
    // run on the server thread, even while paused; futures carry completion or exceptions;
    // pending requests are abandoned when the server stops
    std::future<void> executeCommand(std::string command);
    std::future<void> save();
    std::future<std::vector<std::string>> getPlayerNames();

    // saves world and player, then joins the server thread
    void stop();

    // stops ticking the world, like the game's pause menu
    void setPaused(bool paused);

    bool isPaused() const;

    bool isRunning() const;

    unsigned short getPort() const;

private:
    void _run(EmbeddedServerConfig config, std::shared_ptr<std::promise<bool>> started) noexcept;
    void _enqueue(std::function<void(class ServerNetworkHandler &)> task);

    std::thread mThread;
    std::shared_ptr<ILogEndPoint> mLogEndPoint;
    std::atomic<bool> mStopRequested{false};
    std::atomic<bool> mPaused{false};
    std::atomic<unsigned short> mPort{0};
    mutable std::mutex mMutex;
    State mState = State::Stopped;
    Error mError;
    Progress mProgress = Progress::None;
    std::vector<std::function<void(class ServerNetworkHandler &)>> mTasks;
};
