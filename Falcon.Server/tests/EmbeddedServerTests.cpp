#include "Falcon/EmbeddedServer.h"
#include "Server/ServerHost.h"
#include "Server/ServerPaths.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Utility/BinaryStream.h"
#include "Network/Client/ClientNetworkSystem.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
    void require(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }
    template<class T> T await(std::future<T> future) {
        require(future.wait_for(std::chrono::seconds(30)) == std::future_status::ready,
                "Request did not complete");
        return future.get();
    }
    int64_t savedTime(const std::filesystem::path &directory) {
        std::ifstream file(directory / "worlds" / "World" / "level.dat", std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        require(bytes.size() > 8, "Missing level.dat payload");
        ReadOnlyBinaryStream stream(bytes.substr(8));
        return NbtIo::readTag(stream, NbtVariant::LittleEndian).getLong("Time");
    }
}

int main() {
    const auto directory = std::filesystem::temp_directory_path()
            / ("falcon-embedded-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto originalRoot = ServerPaths::getRoot();
    try {
        std::atomic<unsigned int> logCalls{0};
        EmbeddedServer server;
        EmbeddedServerConfig config;
        config.dataDirectory = directory;
        config.levelName = "../invalid";
        require(!await(server.startAsync(config)), "Invalid world name accepted");
        require(server.getError().code == EmbeddedServer::ErrorCode::InvalidConfiguration,
                "Invalid configuration error missing");
        require(ServerPaths::getRoot() == originalRoot, "Invalid start changed root");

        std::filesystem::create_directories(directory);
        const auto blocked = directory / "file";
        std::ofstream(blocked) << "not a directory";
        config.levelName = "World";
        config.dataDirectory = blocked;
        require(!await(server.startAsync(config)), "File accepted as data directory");
        require(!server.getError().message.empty(), "Filesystem exception was not reported");
        require(ServerPaths::getRoot() == originalRoot, "Failed start changed root");

        config.dataDirectory = directory / "new-directory";
        config.properties = {{"view-distance", "5"}, {"tick-distance", "4"},
                             {"level-seed", "1"}, {"auto-compaction-interval", "0"}};
        config.log = [&logCalls](LogLevel, const std::string &) {
            logCalls.fetch_add(1);
            throw std::runtime_error("Host logger failure");
        };
        require(await(server.startAsync(config)), "Server did not start");
        require(server.isRunning() && server.getPort() != 0, "Running state or port missing");
        require(std::filesystem::exists(config.dataDirectory / "server.properties"),
                "Properties were not written on first start");

        EmbeddedServer other;
        auto otherConfig = config;
        otherConfig.log = {};
        otherConfig.dataDirectory = directory / "other";
        require(!await(other.startAsync(otherConfig)), "Second server started");
        require(other.getError().code == EmbeddedServer::ErrorCode::Busy, "Busy error missing");
        require(server.isRunning(), "Rejected instance stopped active server");
        require(ServerPaths::getRoot() == config.dataDirectory, "Rejected instance changed root");
        ServerHost competingHost;
        require(!competingHost.acquire(), "Dedicated host bypassed process guard");

        server.setPaused(true);
        require(server.isPaused(), "Pause state missing");
        await(server.executeCommand("time set 1000"));
        await(server.save());
        require(await(server.getPlayerNames()).empty(), "Unexpected players");
        require(savedTime(config.dataDirectory) == 1000, "Command did not execute during pause");
        const auto metadata = config.dataDirectory / "worlds" / "World" / "level.dat";
        std::filesystem::remove(metadata);
        std::filesystem::create_directory(metadata);
        bool saveRejected = false;
        try {
            await(server.save());
        } catch (const std::exception &) {
            saveRejected = true;
        }
        std::filesystem::remove(metadata);
        require(saveRejected, "Save future ignored a disk error");
        require(server.isRunning(), "Save failure stopped the server");
        await(server.save());
        ClientConnectionSettings client;
        client.mHost = "127.0.0.1";
        client.mPort = server.getPort();
        client.mProtocolVersion = 2225;
        client.mGameVersion = "1.26.60";
        client.mTransportLayer = TransportLayer::NetherNet;
        client.mNetherNet.mSignalingType = NetherNetSignalingType::Http;
        client.mIdentity.mDisplayName = "EmbeddedPlayer";
        client.mChunkRadius = 5;
        client.mDeferSpawn = false;
        client.mTimeoutMs = 30000;
        auto connection = ClientNetworkSystem::dial(client);
        require(connection.mConnection != nullptr, connection.mError.c_str());
        const auto players = await(server.getPlayerNames());
        require(players.size() == 1 && players.front() == "EmbeddedPlayer", "Player snapshot missing");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        await(server.save());
        require(savedTime(config.dataDirectory) == 1000, "Paused world time advanced");
        connection.mConnection->disconnect("Done");
        server.setPaused(false);
        server.requestStop();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (server.getState() == EmbeddedServer::State::Stopping
               && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        require(server.getState() == EmbeddedServer::State::Stopped, "Asynchronous stop did not finish");
        require(!server.isRunning() && server.getPort() == 0, "Stop did not clear state");
        require(ServerPaths::getRoot() == originalRoot, "Stop did not restore root");
        require(std::filesystem::exists(config.dataDirectory / "worlds" / "World" / "level.dat"),
                "World was not saved");
        bool rejected = false;
        try {
            await(server.save());
        } catch (...) {
            rejected = true;
        }
        require(rejected, "Stopped server accepted request");
        const unsigned int callsAfterStop = logCalls.load();
        require(callsAfterStop != 0, "Log callback was not called");
        require(await(other.startAsync(otherConfig)), "Process guard was not released");
        other.stop();
        require(logCalls.load() == callsAfterStop, "Stopped server retained log callback");
        server.stop();
        auto invalidProperties = config;
        invalidProperties.properties["view-distance"] = "-1";
        require(!await(server.startAsync(invalidProperties)), "Invalid property accepted");
        require(server.getError().code == EmbeddedServer::ErrorCode::InvalidConfiguration,
                "Invalid property error missing");
        require(await(server.startAsync(config)), "Same instance could not restart");
        server.stop();
        const auto worldProperties = config.dataDirectory / "players" / "world_dynamic_properties.dat";
        std::filesystem::remove(worldProperties);
        std::filesystem::create_directory(worldProperties);
        require(await(server.startAsync(config)), "Could not restart for shutdown failure check");
        server.stop();
        require(server.getState() == EmbeddedServer::State::Failed,
                "World properties save failure reported a clean shutdown");
        require(server.getError().code == EmbeddedServer::ErrorCode::Shutdown,
                "World properties save failure did not report Shutdown");
        std::filesystem::remove(worldProperties);
        require(await(server.startAsync(config)), "Could not restart after failed shutdown");
        server.setPaused(true);
        client.mPort = server.getPort();
        auto shutdownConnection = ClientNetworkSystem::dial(client);
        require(shutdownConnection.mConnection != nullptr, shutdownConnection.mError.c_str());
        const auto playerData = config.dataDirectory / "players" / "embeddedplayer.dat";
        std::filesystem::remove(playerData);
        std::filesystem::create_directory(playerData);
        server.stop();
        require(server.getState() == EmbeddedServer::State::Failed,
                "Player save failure reported a clean shutdown");
        require(server.getError().code == EmbeddedServer::ErrorCode::Shutdown,
                "Player save failure did not report Shutdown");
        shutdownConnection.mConnection->disconnect("Done");
        std::filesystem::remove(playerData);
        std::filesystem::remove_all(directory);
        std::cout << "Embedded lifecycle checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        LOG_ERROR(LogAreaID::Server, "%s", error.what());
        BedrockLog::flush();
        return 1;
    }
}
