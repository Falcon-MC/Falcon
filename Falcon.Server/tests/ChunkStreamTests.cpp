#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/RequestChunkRadiusPacket.h"
#include "Server/ServerHost.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <set>
#include <string>

class ServerTickProbe {
public:
    static void nextChunkStreamTick(ServerNetworkHandler &owner) {
        ++owner.mChunkStreamTick;
    }
};

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    int32_t grant(ServerNetworkHandler &handler, const NetworkIdentifier &id, ServerPlayer &player, int32_t radius) {
        RequestChunkRadiusPacket request;
        request.mRadius = radius;
        request.mMaxRadius = (int32_t) std::min(radius, 255);
        request.handle(id, handler);
        return player.getChunkStreamState().mRadius;
    }

    // a player crossing chunks while some wait to be sent: each one goes out once
    void flyingSendsEachChunkOnce() {
        ServerNetworkHandler owner("Chunk stream", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        const int32_t radius = level.getViewDistance();
        const int32_t steps = 20;
        for (int32_t x = -radius - 2; x <= steps + radius + 2; x++) {
            for (int32_t z = -radius - 2; z <= radius + 2; z++)
                level.insertChunk(LevelChunk(x, z)).setPopulated(true);
        }

        // enough players online that each one gets the smallest send budget, short of the view
        ServerPlayer *flyer = nullptr;
        for (uint32_t i = 0; i < 40; i++) {
            const NetworkIdentifier id("stream", 100 + i);
            ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 5000 + i, nullptr).first->second;
            if (i == 0)
                flyer = &player;
        }
        flyer->setName("Flyer");
        flyer->setLoginState(ServerPlayer::LoginState::Spawned);
        flyer->setSpawnChunksReady(true);
        flyer->moveToLevel(level);
        flyer->getChunkStreamState().mRadius = radius;

        std::set<int64_t> crossed;
        for (int32_t step = 0; step <= steps; step++) {
            flyer->setPosition(Vector3f((float) step * 16.0f + 8.0f, 100.0f, 8.0f));
            for (int32_t dx = -radius; dx <= radius; dx++) {
                for (int32_t dz = -radius; dz <= radius; dz++) {
                    if (dx * dx + dz * dz <= radius * radius)
                        crossed.insert(ChunkStreamHandler::packChunk(step + dx, dz));
                }
            }
            ServerTickProbe::nextChunkStreamTick(owner);
            ChunkStreamHandler::tick(owner, *flyer);
        }
        require(flyer->getSentChunkCount() > 0, "no chunk was sent to the flying player");
        require(flyer->getSentChunkCount() <= crossed.size(),
                "a chunk was sent twice: " + std::to_string(flyer->getSentChunkCount()) + " sends for "
                        + std::to_string(crossed.size()) + " chunks crossed");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path root =
            std::filesystem::temp_directory_path()
            / ("falcon-chunk-stream-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(root);
        ServerHost host;
        require(host.acquire(), "could not acquire the host");
        ServerPaths::setRoot(root);
        ServerHostOptions options;
        options.bindAddress = "127.0.0.1";
        options.portOverride = 0;
        options.plugins = false;
        options.properties = {{"level-name", "World"},          {"level-seed", "1"},
                              {"max-threads", "1"},             {"online-mode", "false"},
                              {"network-encryption", "false"}, {"view-distance", "32"},
                              {"tick-distance", "4"},           {"auto-compaction-interval", "0"}};
        require(host.start(options), "could not start the host");
        ServerNetworkHandler &handler = host.getHandler();
        require(handler.getLevel().getViewDistance() == 32, "view-distance 32 was cut down");

        const NetworkIdentifier id("radius", 1);
        ServerPlayer &player = handler.getPlayers().try_emplace(id, id, 9999, nullptr).first->second;
        player.setName("RadiusPlayer");
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.moveToLevel(handler.getLevel());

        // the game grants what is asked, between 5 chunks and view-distance
        require(grant(handler, id, player, 32) == 32, "a 32 chunk request under view-distance 32 was cut down");
        require(grant(handler, id, player, 20) == 20, "a 20 chunk request was not granted as asked");
        require(grant(handler, id, player, 64) == 32, "a request beyond view-distance was not brought back to it");
        require(grant(handler, id, player, 2) == 5, "a request under 5 chunks was not raised to 5");
        flyingSendsEachChunkOnce();
        std::cout << "chunk stream checks passed\n";
        host.stop();
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
    return result;
}
