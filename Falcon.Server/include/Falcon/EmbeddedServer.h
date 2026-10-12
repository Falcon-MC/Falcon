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
    // Holds server.properties, the worlds folder, player data and packs, like a dedicated server's directory.
    std::filesystem::path dataDirectory;
    // The world to play, a folder under dataDirectory/worlds. It is created when it does not exist yet.
    std::string levelName;
    // Extra server.properties values, such as gamemode, difficulty or level-seed.
    std::map<std::string, std::string> properties;
    std::function<void(LogLevel level, const std::string &message)> log;
};

/**
 * A server running inside another program, on its own thread, for a single player on the same machine. It
 * listens on the loopback address alone, on a port the system picks, and lets the player in without an Xbox
 * login or encryption. It never loads plugins and never touches the process: no console, no signal handlers,
 * no exit. One runs at a time.
 */
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

    /**
     * Starts the server and waits until it listens or failed to. Returns false when it could not start.
     */
    bool start(const EmbeddedServerConfig &config);
    // Poll the future and getState() from the UI; start()/stop() are blocking wrappers.
    // Serialize lifecycle calls. Log callbacks run on the logger thread.
    std::future<bool> startAsync(const EmbeddedServerConfig &config);
    // Returns immediately; Stopped/Failed means cleanup has completed.
    void requestStop();
    State getState() const;
    Progress getProgress() const;
    Error getError() const;
    // Requests execute on the server thread, including during pause. Futures report
    // completion or exceptions; pending requests are abandoned when the server stops.
    std::future<void> executeCommand(std::string command);
    std::future<void> save();
    std::future<std::vector<std::string>> getPlayerNames();

    /**
     * Saves the world and the player, stops the server and waits for its thread.
     */
    void stop();

    /**
     * A paused server stops ticking the world, the way the game freezes while its pause menu is open.
     */
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
