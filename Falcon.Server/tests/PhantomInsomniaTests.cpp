#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Hostile/PhantomActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void counter(ServerPlayer &player) {
        require(player.getTimeSinceRest() == 1, "a new player starts away from 1");
        player.tickInsomnia(true);
        player.tickInsomnia(true);
        require(player.getTimeSinceRest() == 3, "awake ticks were not counted");
        player.tickInsomnia(false);
        require(player.getTimeSinceRest() == 1, "doinsomnia off did not reset the timer");

        player.setTimeSinceRest(500);
        player.setSleeping(Vector3i(0, 100, 0));
        player.tickInsomnia(true);
        require(player.getTimeSinceRest() == 1, "sleeping did not reset the timer");
        player.clearSleeping();

        player.setTimeSinceRest(500);
        player.setGameType((int32_t) GameType::Spectator);
        player.tickInsomnia(true);
        player.tickInsomnia(false);
        require(player.getTimeSinceRest() == 500, "a spectator's timer moved");
        player.setGameType((int32_t) GameType::Survival);
    }

    void placement(ServerNetworkHandler &owner, Level &overworld, ServerPlayer &player) {
        const MobActor *phantom = ActorClassRegistry::getMobPrototype(PhantomActor::IDENTIFIER);
        require(phantom != nullptr, "no phantom class");
        std::mt19937 random(7);

        player.setTimeSinceRest(1);
        Vector3f spot(5.5f, 70.0f, 5.5f);
        for (int32_t attempt = 0; attempt < 50; ++attempt)
            require(!phantom->placeNaturalSpawn(owner, overworld, spot, random), "a rested player drew a phantom");

        player.setTimeSinceRest(std::numeric_limits<int32_t>::max());
        Vector3f far(150.5f, 70.0f, 0.5f);
        require(!phantom->placeNaturalSpawn(owner, overworld, far, random), "a spot 150 blocks away drew a phantom");

        require(phantom->placeNaturalSpawn(owner, overworld, spot, random), "a player awake for ages drew nothing");
        require(spot.x >= -10.0f && spot.x <= 10.0f && spot.z >= -10.0f && spot.z <= 10.0f,
                "the phantom left the 21 by 21 square around the player");
        require(spot.y >= 120.0f && spot.y <= 134.0f, "the phantom is not 20 to 34 blocks above the player");

        // five phantoms around already: no sixth
        for (int32_t index = 0; index < 5; ++index)
            require(owner.spawnActor(overworld, PhantomActor::IDENTIFIER, Vector3f((float) index, 125.0f, 0.5f)) != nullptr,
                    "no phantom to crowd the sky");
        Vector3f crowded(5.5f, 70.0f, 5.5f);
        for (int32_t attempt = 0; attempt < 20; ++attempt)
            require(!phantom->placeNaturalSpawn(owner, overworld, crowded, random), "a sixth phantom joined the crowd");

        Level &nether = owner.getWorlds().getDefault().getLevel(DimensionType::Nether);
        Vector3f below(5.5f, 70.0f, 5.5f);
        require(!phantom->placeNaturalSpawn(owner, nether, below, random), "a phantom spawned without a sky");
    }

    void run() {
        ServerNetworkHandler owner("Phantom insomnia", "", 1);
        EntityDefinitions::initialize();
        Level &overworld = owner.getWorlds().getDefault().getOverworld();
        for (int32_t x = -1; x <= 1; ++x) {
            for (int32_t z = -1; z <= 1; ++z)
                overworld.insertChunk(LevelChunk(x, z)).setPopulated(true);
        }

        const NetworkIdentifier id("9901", 9901);
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 9901).first->second;
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(overworld);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        player.setLoginState(ServerPlayer::LoginState::Spawned);

        counter(player);
        placement(owner, overworld, player);
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-phantom-insomnia-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
        std::cout << "phantom insomnia checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
