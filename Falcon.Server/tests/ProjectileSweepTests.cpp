#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ServerActor &snowball(ServerNetworkHandler &owner, Level &level, float x) {
        ServerActor *actor = owner.spawnActor(level, "minecraft:snowball", Vector3f(x, 100.5f, 0.5f));
        require(actor != nullptr, "no fixture snowball");
        actor->setProjectile(true);
        return *actor;
    }

    bool exists(ServerNetworkHandler &owner, int64_t uniqueId) {
        return owner.getActors().find(uniqueId) != owner.getActors().end();
    }

    // a fast sweep still meets the mob it crosses; a non finite one is dropped instead of walked
    void run() {
        ServerNetworkHandler owner("Projectile sweep", "", 1);
        EntityDefinitions::initialize();
        Level &level = owner.getWorlds().getDefault().getOverworld();
        for (int32_t chunkZ = 0; chunkZ <= 3; ++chunkZ)
            level.insertChunk(LevelChunk(0, chunkZ));
        const BlockState stone = VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState();
        for (int32_t x = 0; x < 16; ++x) {
            for (int32_t z = 0; z < 64; ++z)
                level.setBlock(Vector3i(x, 99, z), stone, false);
        }

        ServerActor &fast = snowball(owner, level, 0.5f);
        ServerActor &broken = snowball(owner, level, 8.5f);
        ServerActor *cow = owner.spawnActor(level, "minecraft:cow", Vector3f(0.5f, 100.0f, 20.5f));
        require(cow != nullptr, "no fixture cow");
        const int64_t fastId = fast.getUniqueId();
        const int64_t brokenId = broken.getUniqueId();

        // a thrown projectile only hits from its third tick
        for (int tick = 0; tick < 2; ++tick) {
            fast.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
            fast.setPosition(Vector3f(0.5f, 100.5f, 0.5f));
            broken.setMotion(Vector3f(0.0f, 0.0f, 0.0f));
            broken.setPosition(Vector3f(8.5f, 100.5f, 0.5f));
            owner.tickActors();
        }
        require(exists(owner, fastId) && exists(owner, brokenId), "a resting snowball expired");

        fast.setPosition(Vector3f(0.5f, 100.5f, 0.5f));
        fast.setMotion(Vector3f(0.0f, 0.0f, 40.0f));
        broken.setMotion(Vector3f(0.0f, 0.0f, std::numeric_limits<float>::quiet_NaN()));
        owner.tickActors();
        require(!exists(owner, fastId), "a 40 block sweep went through the cow");
        require(!exists(owner, brokenId), "a non finite projectile stayed in the world");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-projectile-sweep-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Projectile sweep regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
