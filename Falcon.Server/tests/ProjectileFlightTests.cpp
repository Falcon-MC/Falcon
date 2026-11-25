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
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    // the expected values are the game's flight step worked out in float: move by the motion, then
    // motion = motion * inertia (liquid_inertia when the tick starts in water), then motion.y -= gravity;
    // inertia 0.99 and liquid_inertia 0.6 are the loader defaults the arrow and snowball keep
    const float TOLERANCE = 1.0e-4f;

    struct Sample {
        float mY;
        float mZ;
    };

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ServerActor &launch(ServerNetworkHandler &owner, Level &level, const std::string &identifier,
                        const Vector3f &position, const Vector3f &motion) {
        ServerActor *actor = owner.spawnActor(level, identifier, position);
        require(actor != nullptr, "no fixture " + identifier);
        actor->setProjectile(true);
        actor->setRotation(Vector3f(0.0f, 0.0f, 0.0f));
        actor->setMotion(motion);
        return *actor;
    }

    void follow(ServerNetworkHandler &owner, ServerActor &actor, const Sample *expected, int32_t ticks,
                const std::string &name) {
        for (int32_t tick = 0; tick < ticks; ++tick) {
            owner.tickActors();
            const Vector3f position = actor.getPosition();
            const std::string at = name + " tick " + std::to_string(tick + 1) + ": y " + std::to_string(position.y)
                                   + " z " + std::to_string(position.z);
            require(std::fabs(position.x - 0.5f) < TOLERANCE, at + " drifted sideways");
            require(std::fabs(position.y - expected[tick].mY) < TOLERANCE, at + ", want y "
                                                                                  + std::to_string(expected[tick].mY));
            require(std::fabs(position.z - expected[tick].mZ) < TOLERANCE, at + ", want z "
                                                                                  + std::to_string(expected[tick].mZ));
        }
    }

    // an arrow fired level at 3 blocks a tick covers the full 3 blocks first, then sags
    void arrowInAir(ServerNetworkHandler &owner, Level &level) {
        static const Sample EXPECTED[] = {{200.5f, 3.5f},          {200.449997f, 6.47000027f},
                                          {200.350494f, 9.41030025f}, {200.201996f, 12.3211975f},
                                          {200.004974f, 15.2029858f}, {199.759918f, 18.0559559f},
                                          {199.467316f, 20.8803959f}, {199.12764f, 23.6765919f}};
        // the pitch eases a fifth of the way toward the motion each tick
        static const float PITCH[] = {0.0f,      -0.192897f, -0.541948f, -1.01766f,
                                      -1.59634f, -2.25893f,  -2.99004f,  -3.77727f};
        ServerActor &arrow = launch(owner, level, "minecraft:arrow", Vector3f(0.5f, 200.5f, 0.5f),
                                    Vector3f(0.0f, 0.0f, 3.0f));
        for (int32_t tick = 0; tick < 8; ++tick) {
            follow(owner, arrow, &EXPECTED[tick], 1, "arrow");
            const float pitch = arrow.getRotation().x;
            require(std::fabs(pitch - PITCH[tick]) < 1.0e-3f, "arrow tick " + std::to_string(tick + 1) + ": pitch "
                                                                  + std::to_string(pitch) + ", want "
                                                                  + std::to_string(PITCH[tick]));
            require(std::fabs(arrow.getRotation().y) < 1.0e-3f, "arrow flying along z turned its yaw");
        }
        const Vector3f motion = arrow.getMotion();
        require(std::fabs(motion.y + 0.386277f) < TOLERANCE && std::fabs(motion.z - 2.76823f) < TOLERANCE,
                "arrow motion after 8 ticks: y " + std::to_string(motion.y) + " z " + std::to_string(motion.z));
        owner.removeActor(arrow.getUniqueId());
    }

    // a snowball thrown up and forward: gravity 0.03 from its definition
    void snowballInAir(ServerNetworkHandler &owner, Level &level) {
        static const Sample EXPECTED[] = {{201.0f, 2.0f},          {201.464996f, 3.48500013f},
                                          {201.89534f, 4.95515013f}, {202.291382f, 6.41059875f},
                                          {202.653473f, 7.85149288f}, {202.981934f, 9.27797794f},
                                          {203.277115f, 10.6901979f}, {203.539337f, 12.0882959f}};
        ServerActor &snowball = launch(owner, level, "minecraft:snowball", Vector3f(0.5f, 200.5f, 0.5f),
                                       Vector3f(0.0f, 0.5f, 1.5f));
        follow(owner, snowball, EXPECTED, 8, "snowball");
        owner.removeActor(snowball.getUniqueId());
    }

    // an arrow in still water keeps 0.6 of its motion a tick
    void arrowInWater(ServerNetworkHandler &owner, Level &level) {
        static const Sample EXPECTED[] = {{105.5f, 1.5f},          {105.449997f, 2.0999999f},
                                          {105.369995f, 2.46000004f}, {105.271996f, 2.67600012f},
                                          {105.163193f, 2.80560017f}, {105.047913f, 2.88336015f}};
        const BlockState water = VanillaBlocks::fromIdentifier("minecraft:water")->toBlockState();
        for (int32_t y = 100; y <= 110; ++y) {
            for (int32_t x = 0; x < 16; ++x) {
                for (int32_t z = 0; z < 16; ++z)
                    level.setBlock(Vector3i(x, y, z), water, false);
            }
        }
        ServerActor &arrow = launch(owner, level, "minecraft:arrow", Vector3f(0.5f, 105.5f, 0.5f),
                                    Vector3f(0.0f, 0.0f, 1.0f));
        follow(owner, arrow, EXPECTED, 6, "arrow in water");
        owner.removeActor(arrow.getUniqueId());
    }

    void run() {
        ServerNetworkHandler owner("Projectile flight", "", 1);
        EntityDefinitions::initialize();
        Level &level = owner.getWorlds().getDefault().getOverworld();
        for (int32_t chunkZ = 0; chunkZ <= 3; ++chunkZ)
            level.insertChunk(LevelChunk(0, chunkZ));

        arrowInAir(owner, level);
        snowballInAir(owner, level);
        arrowInWater(owner, level);
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-projectile-flight-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Projectile flight regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
