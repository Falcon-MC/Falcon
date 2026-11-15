#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/EyeOfEnderActor.h"
#include "World/Actor/ServerActor.h"
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

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // a thrown eye flies through walls on its way to the stronghold: being inside a block must not hurt it
    void run() {
        ServerNetworkHandler owner("Eye of ender walls", "", 1);
        EntityDefinitions::initialize();
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const BlockState stone = VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState();
        for (int32_t x = 0; x < 16; ++x) {
            for (int32_t y = 90; y < 110; ++y) {
                for (int32_t z = 0; z < 16; ++z)
                    level.setBlock(Vector3i(x, y, z), stone, false);
            }
        }

        ServerActor *eye = owner.spawnActor(level, EyeOfEnderActor::IDENTIFIER, Vector3f(8.5f, 100.0f, 8.5f));
        require(eye != nullptr, "no fixture eye");
        const float health = eye->getHealth();
        for (int32_t tick = 0; tick < 20; ++tick)
            owner.tickActors();
        require(eye->isAlive() && !eye->isDead(), "the eye died inside the stone");
        require(eye->getHealth() == health, "the eye lost health inside the stone");

        require(!eye->hurt(owner, 4.0f, ActorDamageSource::environment("death.attack.generic", eye->getName())),
                "the eye took a hit");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-eye-of-ender-walls-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Eye of ender walls regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
