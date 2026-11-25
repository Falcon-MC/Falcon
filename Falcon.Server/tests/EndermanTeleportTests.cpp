#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // an arrow sends the enderman onto the floor inside its 32 block cube; a snowball is left to hit
    void dodge(const std::filesystem::path &directory) {
        ServerNetworkHandler owner("Enderman teleport", "", 1);
        const std::filesystem::path path = directory / "normal.properties";
        std::ofstream(path) << "difficulty=normal\n";
        owner.setProperties(PropertiesSettings(path.string()));
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        for (int32_t x = -2; x <= 1; x++) {
            for (int32_t z = -2; z <= 1; z++)
                level.insertChunk(LevelChunk(x, z)).setPopulated(true);
        }
        for (int32_t x = -24; x <= 24; ++x) {
            for (int32_t z = -24; z <= 24; ++z)
                level.setBlockState(x, 60, z, BlockState("minecraft:stone"));
        }

        auto *enderman = dynamic_cast<MobActor *>(
                owner.spawnActor(level, "minecraft:enderman", Vector3f(0.5f, 61.0f, 0.5f)));
        require(enderman != nullptr, "the enderman did not spawn");
        enderman->setPosition(Vector3f(0.5f, 61.0f, 0.5f));

        ServerActor *snowball = owner.spawnActor(level, "minecraft:snowball", Vector3f(0.5f, 62.0f, 3.5f));
        require(snowball != nullptr, "the snowball did not spawn");
        require(!enderman->dodgeProjectile(owner, *snowball), "a snowball made the enderman teleport");

        ServerActor *arrow = owner.spawnActor(level, "minecraft:arrow", Vector3f(0.5f, 62.0f, 3.5f));
        require(arrow != nullptr, "the arrow did not spawn");
        require(enderman->dodgeProjectile(owner, *arrow), "the enderman did not dodge the arrow");
        const Vector3f landed = enderman->getPosition();
        require(landed.y == 61.0f, "the enderman did not land on the floor");
        require(std::fabs(landed.x - 0.5f) <= 16.0f && std::fabs(landed.z - 0.5f) <= 16.0f,
                "the enderman left its teleport cube");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory = std::filesystem::temp_directory_path()
                                            / ("falcon-enderman-teleport-"
                                               + std::to_string(
                                                       std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        dodge(directory);
        std::cout << "enderman teleport checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
