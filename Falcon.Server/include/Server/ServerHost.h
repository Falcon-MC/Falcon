#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

class ServerNetworkHandler;

struct ServerHostOptions {
    std::string serverName = "Falcon Server";
    std::string subMotd = "Falcon";
    std::string gameVersion = "1.26.52";
    int protocolVersion = 2193;
    int maxPlayers = 20;
    unsigned short port = 19132;
    unsigned short portV6 = 19133;
    // Replaces the port server.properties asks for, without writing it back to the file. Port 0 lets the
    // system pick a free one, which getPort() then reports.
    std::optional<unsigned short> portOverride;
    // Listens on this IPv4 address alone, such as the loopback address. Empty listens everywhere.
    std::string bindAddress;
    // Written into server.properties before it is read, so they persist like any other setting.
    std::map<std::string, std::string> properties;
    bool plugins = true;
};

/**
 * Brings a server up, ticks it and takes it down without owning the process, so the dedicated executable
 * and a program embedding the server drive it the same way. One host runs at a time; once stopped, another
 * can start in the same process.
 */
class ServerHost {
public:
    ServerHost();

    ~ServerHost();

    bool start(const ServerHostOptions &options);

    void tick();

    /**
     * Saves the players and worlds and closes the connections, but keeps the server's objects alive. The
     * dedicated server ends the process right after, without running destructors that plugin hosts can block.
     */
    void shutdown();

    /**
     * Shuts down, then destroys the server and clears the process wide state, so another world can start.
     */
    void stop();

    bool isRunning() const;

    bool isStopRequested() const;

    ServerNetworkHandler &getHandler();

    unsigned short getPort() const;

private:
    std::unique_ptr<ServerNetworkHandler> mHandler;
    unsigned short mPort = 0;
    bool mShutDown = false;
};
