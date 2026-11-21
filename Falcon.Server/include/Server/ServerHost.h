#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>

class ILogEndPoint;
class ServerNetworkHandler;

struct ServerHostOptions {
    std::string serverName = "Falcon Server";
    std::string subMotd = "Falcon";
    std::string gameVersion = "1.26.60";
    int protocolVersion = 2225;
    int maxPlayers = 20;
    unsigned short port = 19132;
    unsigned short portV6 = 19133;
    // not written back to server.properties; 0 lets the system pick, getPort() reports it
    std::optional<unsigned short> portOverride;
    // IPv4 only (e.g. loopback); empty listens everywhere
    std::string bindAddress;
    // written into server.properties before it is read: they persist like any setting
    std::map<std::string, std::string> properties;
    bool plugins = true;
};

// doesn't own the process: the dedicated executable and an embedder drive it alike;
// one host at a time, another can start once it stops
class ServerHost {
public:
    ServerHost();

    ~ServerHost();

    bool start(const ServerHostOptions &options);
    // reserve before changing process-wide paths; release after stop and path restoration
    bool acquire();
    void release() noexcept;

    void tick(bool paused = false);

    // joining players are answered meanwhile: each login step would otherwise wait a whole tick
    void waitForNextTick(std::chrono::steady_clock::time_point nextTick);

    // keeps the server's objects alive: the dedicated server exits right after,
    // skipping destructors plugin hosts can block
    void shutdown();

    // also clears the process-wide state: another world can start
    void stop();

    bool isRunning() const;

    bool isStopRequested() const;

    ServerNetworkHandler &getHandler();

    unsigned short getPort() const;

private:
    void _closeContentLog();

    std::unique_ptr<ServerNetworkHandler> mHandler;
    std::shared_ptr<ILogEndPoint> mContentLogEndPoint;
    unsigned short mPort = 0;
    bool mShutDown = false;
    bool mOwnsProcess = false;
};
