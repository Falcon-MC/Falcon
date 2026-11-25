#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/BlockClip.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <cmath>
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

    // the segment stops on the face it meets first, and passes over the empty half of a slab
    void clipFaces(Level &level) {
        const BlockState stone = VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState();
        level.setBlock(Vector3i(2, 105, 10), stone, false);
        BlockClipHit hit;
        require(BlockClip::clip(level, Vector3f(2.5f, 105.5f, 0.5f), Vector3f(2.5f, 105.5f, 30.5f), nullptr, hit),
                "a segment went through a stone block");
        require(hit.mBlock.x == 2 && hit.mBlock.y == 105 && hit.mBlock.z == 10, "the clip struck the wrong block");
        require(hit.mFace == 2 && std::fabs(hit.mPosition.z - 10.0f) < 0.0001f,
                "the clip did not stop on the north face, z " + std::to_string(hit.mPosition.z));

        require(BlockClip::clip(level, Vector3f(12.5f, 105.25f, 10.5f), Vector3f(-4.5f, 105.75f, 10.5f), nullptr, hit),
                "a segment along -x went through a stone block");
        require(hit.mFace == 5 && std::fabs(hit.mPosition.x - 3.0f) < 0.0001f,
                "the clip did not stop on the east face");

        BlockState slab = VanillaBlocks::fromIdentifier("minecraft:smooth_stone_slab")->toBlockState();
        slab.mStates.putString("minecraft:vertical_half", "bottom");
        level.setBlock(Vector3i(6, 105, 10), slab, false);
        require(!BlockClip::clip(level, Vector3f(6.5f, 105.75f, 0.5f), Vector3f(6.5f, 105.75f, 30.5f), nullptr, hit),
                "a segment over a bottom slab struck it");
        require(BlockClip::clip(level, Vector3f(6.5f, 105.25f, 0.5f), Vector3f(6.5f, 105.25f, 30.5f), nullptr, hit),
                "a segment through a bottom slab missed it");
    }

    // an arrow faster than a block per tick stops in a one block wall, on its face
    void arrowIntoWall(ServerNetworkHandler &owner, Level &level) {
        const BlockState stone = VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState();
        for (int32_t y = 98; y <= 102; ++y)
            level.setBlock(Vector3i(12, y, 30), stone, false);
        ServerActor *arrow = owner.spawnActor(level, "minecraft:arrow", Vector3f(12.5f, 100.5f, 0.5f));
        require(arrow != nullptr, "no fixture arrow");
        arrow->setProjectile(true);
        for (int tick = 0; tick < 2; ++tick) {
            arrow->setMotion(Vector3f(0.0f, 0.0f, 0.0f));
            arrow->setPosition(Vector3f(12.5f, 100.5f, 0.5f));
            owner.tickActors();
        }
        arrow->setMotion(Vector3f(0.0f, 0.0f, 40.0f));
        owner.tickActors();
        require(arrow->getProjectileData().mStuck, "a fast arrow went through a one block wall");
        require(std::fabs(arrow->getPosition().z - 30.0f) < 0.001f,
                "the arrow did not stop on the wall's face, z " + std::to_string(arrow->getPosition().z));
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

        clipFaces(level);
        arrowIntoWall(owner, level);
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
