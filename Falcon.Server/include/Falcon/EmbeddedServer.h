#pragma once

#include "Core/Debug/LogLevel.h"

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

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
    EmbeddedServer();

    ~EmbeddedServer();

    EmbeddedServer(const EmbeddedServer &) = delete;

    EmbeddedServer &operator=(const EmbeddedServer &) = delete;

    /**
     * Starts the server and waits until it listens or failed to. Returns false when it could not start.
     */
    bool start(const EmbeddedServerConfig &config);

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
    enum class State {
        Starting,
        Running,
        Failed,
        Stopped
    };

    void _run(EmbeddedServerConfig config);

    std::thread mThread;
    std::shared_ptr<ILogEndPoint> mLogEndPoint;
    std::atomic<bool> mStopRequested{false};
    std::atomic<bool> mPaused{false};
    std::atomic<unsigned short> mPort{0};
    mutable std::mutex mMutex;
    std::condition_variable mStateChanged;
    State mState = State::Stopped;
};
