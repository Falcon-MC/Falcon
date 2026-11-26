#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const char *const STATE_FACES = "multi_face_direction_bits";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    int bitCount(int bits) {
        int count = 0;
        for (; bits != 0; bits &= bits - 1)
            ++count;
        return count;
    }

    struct Arena {
        ServerNetworkHandler mOwner{"BoneMealSpread", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            mLevel.setOwner(&mOwner);
            mLevel.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        }

        void set(int32_t x, int32_t y, int32_t z, const BlockState &state) {
            mLevel.setBlockState(x, y, z, state);
        }

        bool fertilize(const Vector3i &position) {
            const BlockState state = mLevel.getBlockState(position.x, position.y, position.z);
            return VanillaBlocks::fromState(state)->onFertilized(mOwner, mLevel, position, state);
        }

        int lichenFaces() {
            int faces = 0;
            for (int32_t x = 4; x <= 12; ++x)
                for (int32_t y = 66; y <= 74; ++y)
                    for (int32_t z = 4; z <= 12; ++z) {
                        const BlockState state = mLevel.getBlockState(x, y, z);
                        if (state.mName == BlockNames::GLOW_LICHEN)
                            faces += bitCount(state.mStates.getInt(STATE_FACES, 0));
                    }
            return faces;
        }
    };

    BlockState lichen(int bits) {
        BlockState state = VanillaBlocks::GLOW_LICHEN().toBlockState();
        state.mStates.putInt(STATE_FACES, bits);
        return state;
    }

    // each bone meal grows exactly one face on a stone floor
    void lichenSpreads() {
        Arena arena;
        const BlockState stone = VanillaBlocks::STONE().toBlockState();
        for (int32_t x = 4; x <= 12; ++x)
            for (int32_t z = 4; z <= 12; ++z)
                arena.set(x, 69, z, stone);
        const Vector3i position(8, 70, 8);
        arena.set(position.x, position.y, position.z,
                  lichen(PlacementOrientation::multiFaceBit(PlacementOrientation::FACE_DOWN)));

        for (int use = 1; use <= 3; ++use) {
            require(arena.fertilize(position), "bone meal did not take on glow lichen");
            require(arena.lichenFaces() == 1 + use, "bone meal grew " + std::to_string(arena.lichenFaces() - 1)
                                                            + " faces after " + std::to_string(use) + " uses");
        }
    }

    // a lichen on every face inside a solid stone cube has nowhere to go, not even around a corner: the bone meal
    // is kept
    void lichenBoxedIn() {
        Arena arena;
        const BlockState stone = VanillaBlocks::STONE().toBlockState();
        const Vector3i position(8, 70, 8);
        for (int32_t x = -1; x <= 1; ++x)
            for (int32_t y = -1; y <= 1; ++y)
                for (int32_t z = -1; z <= 1; ++z)
                    arena.set(position.x + x, position.y + y, position.z + z, stone);
        arena.set(position.x, position.y, position.z, lichen(63));
        require(!arena.fertilize(position), "a boxed-in lichen took the bone meal");
    }

    // a pickle on coral grows to four and seeds the watered coral around it; off coral or dead it takes nothing
    void seaPickles() {
        Arena arena;
        const BlockState coral = VanillaBlocks::BRAIN_CORAL_BLOCK().toBlockState();
        const BlockState water = VanillaBlocks::WATER().toBlockState();
        for (int32_t x = 4; x <= 12; ++x)
            for (int32_t z = 4; z <= 12; ++z) {
                arena.set(x, 69, z, coral);
                arena.set(x, 70, z, water);
            }
        const Vector3i position(8, 70, 8);
        BlockState pickle = VanillaBlocks::SEA_PICKLE().toBlockState();
        arena.set(position.x, position.y, position.z, pickle);
        arena.mLevel.setBlockStateAtLayer(position.x, position.y, position.z, 1, water);

        for (int use = 0; use < 8; ++use)
            require(arena.fertilize(position), "bone meal did not take on a pickle on coral");
        require(arena.mLevel.getBlockState(position.x, position.y, position.z).mStates.getInt("cluster_count", 0) == 3,
                "the bone mealed pickle is not four");

        int seeded = 0;
        for (int32_t x = 4; x <= 12; ++x)
            for (int32_t z = 4; z <= 12; ++z) {
                if (Vector3i(x, 70, z) == position
                    || arena.mLevel.getBlockState(x, 70, z).mName != BlockNames::SEA_PICKLE)
                    continue;
                ++seeded;
                require(std::abs(x - position.x) + std::abs(z - position.z) <= 2, "a pickle was seeded out of reach");
                require(arena.mLevel.getBlockStateAtLayer(x, 70, z, 1).mName == water.mName,
                        "a seeded pickle lost its water");
            }
        require(seeded > 0, "eight bone meals seeded no pickle");

        const Vector3i dry(12, 70, 12);
        arena.set(dry.x, 69, dry.z, VanillaBlocks::STONE().toBlockState());
        arena.set(dry.x, dry.y, dry.z, pickle);
        require(!arena.fertilize(dry), "a pickle off coral took the bone meal");

        pickle.mStates.putBool("dead_bit", true);
        arena.set(4, 70, 4, pickle);
        require(!arena.fertilize(Vector3i(4, 70, 4)), "a dead pickle took the bone meal");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-bone-meal-spread-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        lichenSpreads();
        lichenBoxedIn();
        seaPickles();
        std::cout << "bone meal spread checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
