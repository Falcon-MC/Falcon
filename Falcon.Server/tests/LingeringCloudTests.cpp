#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // a mob standing in a lingering cloud gets its effect, like a player does
    void run() {
        ServerNetworkHandler owner("Lingering cloud", "", 1);
        EntityDefinitions::initialize();
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const BlockState stone = VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState();
        for (int32_t x = 0; x < 16; ++x) {
            for (int32_t z = 0; z < 16; ++z)
                level.setBlock(Vector3i(x, 99, z), stone, false);
        }

        ServerActor *cow = owner.spawnActor(level, "minecraft:cow", Vector3f(8.5f, 100.0f, 8.5f));
        require(cow != nullptr, "no fixture cow");
        ServerNetworkHandler::AreaEffectCloudSettings settings;
        // potion 17: slowness
        settings.mPotionId = 17;
        settings.mRadius = 3.0f;
        ServerActor *cloud = owner.spawnAreaEffectCloud(level, Vector3f(8.5f, 100.0f, 8.5f), settings);
        require(cloud != nullptr, "no fixture cloud");

        owner.tickActors();
        require(cow->hasEffect(MobEffectId::Slowness), "the cow in the cloud got no effect");
        require(!cloud->hasEffect(MobEffectId::Slowness), "the cloud applied its potion to itself");

        const std::unique_ptr<ServerActor> reloaded = ActorClassRegistry::create(1, "minecraft:area_effect_cloud");
        require(reloaded != nullptr && !reloaded->shouldSave(), "a cloud is saved without the state that drives it");
        require(ActorClassRegistry::getSize("minecraft:area_effect_cloud").mWidth == 3.0f, "the cloud lost its size");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-lingering-cloud-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Lingering cloud regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
