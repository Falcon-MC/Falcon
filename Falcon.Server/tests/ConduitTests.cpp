#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ConduitBlockActor.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    const Vector3i CONDUIT(8, 100, 8);

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool onFrameRing(int32_t x, int32_t y, int32_t z) {
        const int32_t zeros = (x == 0) + (y == 0) + (z == 0);
        return zeros >= 1 && (std::abs(x) == 2 || std::abs(y) == 2 || std::abs(z) == 2);
    }

    // water around the conduit and prismarine on its three rings
    void build(Level &level) {
        for (int32_t x = -2; x <= 2; ++x) {
            for (int32_t y = -2; y <= 2; ++y) {
                for (int32_t z = -2; z <= 2; ++z) {
                    const char *block = onFrameRing(x, y, z) ? "minecraft:prismarine" : "minecraft:water";
                    level.setBlock(Vector3i(CONDUIT.x + x, CONDUIT.y + y, CONDUIT.z + z), BlockState(block), false);
                }
            }
        }
        level.setBlock(CONDUIT, BlockState("minecraft:conduit"), false);
    }

    void checkWater(Level &level) {
        build(level);
        require(ConduitBlockActor::isSurroundedByWater(level, CONDUIT), "water all around was not enough");
        const Vector3i corner(CONDUIT.x + 1, CONDUIT.y + 1, CONDUIT.z + 1);
        level.setBlock(corner, BlockState("minecraft:stone"), false);
        require(!ConduitBlockActor::isSurroundedByWater(level, CONDUIT), "a stone corner still counted as water");
        level.setBlock(corner, BlockState("minecraft:water"), false);
    }

    void checkFrame(Level &level) {
        std::vector<Vector3i> frame;
        ConduitBlockActor::collectFrame(level, CONDUIT, frame);
        require(frame.size() == 42, "a full frame did not count 42 blocks");

        level.setBlock(Vector3i(CONDUIT.x + 2, CONDUIT.y, CONDUIT.z), BlockState("minecraft:sea_lantern"), false);
        level.setBlock(Vector3i(CONDUIT.x, CONDUIT.y + 2, CONDUIT.z + 1), BlockState("minecraft:stone"), false);
        frame.clear();
        ConduitBlockActor::collectFrame(level, CONDUIT, frame);
        require(frame.size() == 41, "a sea lantern was not a frame block, or stone was");

        require(ConduitBlockActor::rangeFor(16) == 32 && ConduitBlockActor::rangeFor(41) == 80
                        && ConduitBlockActor::rangeFor(42) == 96,
                "the range is not 16 blocks per 7 frame blocks");
        build(level);
    }

    void checkTick(ServerNetworkHandler &server, Level &level) {
        build(level);
        ConduitBlockActor &conduit = level.getBlockActors().getOrCreate<ConduitBlockActor>(CONDUIT);
        conduit.tick(server);
        require(conduit.isActive() && conduit.isHunting() && conduit.getEffectRange() == 96,
                "a full frame in water did not activate and hunt");

        level.setBlock(Vector3i(CONDUIT.x - 1, CONDUIT.y, CONDUIT.z), BlockState("minecraft:stone"), false);
        for (int32_t tick = 0; tick < ConduitBlockActor::UPDATE_TICKS; ++tick)
            conduit.tick(server);
        require(!conduit.isActive() && !conduit.isHunting(), "a conduit out of water stayed active");
    }
}

int main() {
    try {
        ServerNetworkHandler server("Conduit", "", 1);
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        checkWater(level);
        checkFrame(level);
        checkTick(server, level);
    } catch (const std::exception &error) {
        std::clog << "Conduit regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
