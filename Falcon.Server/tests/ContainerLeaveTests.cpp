#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

class ServerTickProbe {
public:
    static void leave(ServerNetworkHandler &owner, const NetworkIdentifier &id) {
        owner.onConnectionClosed(id, DisconnectFailReason::Disconnected, "");
    }
};

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // a player who leaves with a chest open no longer counts as looking into it
    void run() {
        ServerNetworkHandler owner("Container leave", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i position(2, 100, 2);
        level.setBlock(position, VanillaBlocks::fromIdentifier("minecraft:chest")->toBlockState(), false);
        ChestBlockActor &chest = level.getBlockActors().getOrCreate<ChestBlockActor>(position);

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 4001).first->second;
        player.setName("Leaver");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(2.5f, 100.0f, 0.5f));
        player.getInventoryManager().attach(&player, &owner);

        require(player.getInventoryManager().onClientOpenChest(position), "the chest did not open");
        require(chest.getViewerCount() == 1, "the open chest has no viewer");

        ServerTickProbe::leave(owner, id);
        require(owner.getPlayers().find(id) == owner.getPlayers().end(), "the player stayed after leaving");
        require(chest.getViewerCount() == 0, "the chest still counts the player who left");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-container-leave-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Container leave regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
