#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/MultifaceBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

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

    void placement() {
        Level level("multiface-test", 2);
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i position(8, 20, 8);
        const BlockState stone = VanillaBlocks::STONE().toBlockState();
        const BlockState shortGrass = VanillaBlocks::SHORT_GRASS().toBlockState();
        const BlockState air = VanillaBlocks::AIR().toBlockState();

        for (const char *name: {BlockNames::SCULK_VEIN, BlockNames::GLOW_LICHEN, BlockNames::RESIN_CLUMP}) {
            const auto *block = dynamic_cast<const MultifaceBlock *>(VanillaBlocks::fromIdentifier(name));
            require(block != nullptr, std::string(name) + " is not a multiface block");

            for (int face = 0; face < 6; ++face) {
                const Vector3i support = PlacementOrientation::relativePosition(position, face ^ 1);
                level.setBlock(support, stone, false);
                require(block->canPlaceAt(level, position, face), std::string(name) + " rejected a full face");
                level.setBlock(support, shortGrass, false);
                require(!block->canPlaceAt(level, position, face), std::string(name) + " clung to short grass");
                level.setBlock(support, air, false);
                require(!block->canPlaceAt(level, position, face), std::string(name) + " clung to air");
            }
        }
    }

    // a vein on the floor and the north wall
    void support() {
        ServerNetworkHandler owner("Multiface", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.setOwner(&owner);
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        const Vector3i position(8, 70, 8);
        const Vector3i floor(8, 69, 8);
        const Vector3i wall(8, 70, 7);
        const BlockState stone = VanillaBlocks::STONE().toBlockState();
        const BlockState air = VanillaBlocks::AIR().toBlockState();
        level.setBlockState(floor.x, floor.y, floor.z, stone);
        level.setBlockState(wall.x, wall.y, wall.z, stone);

        BlockState vein = VanillaBlocks::SCULK_VEIN().toBlockState();
        vein.mStates.putInt("multi_face_direction_bits",
                            PlacementOrientation::multiFaceBit(PlacementOrientation::FACE_DOWN)
                                    | PlacementOrientation::multiFaceBit(PlacementOrientation::FACE_NORTH));
        level.setBlockState(position.x, position.y, position.z, vein);
        const Block *block = VanillaBlocks::fromIdentifier(BlockNames::SCULK_VEIN);

        block->onNeighbourChanged(owner, level, position, vein);
        require(level.getBlockState(position.x, position.y, position.z) == vein, "a supported vein changed");

        level.setBlockState(wall.x, wall.y, wall.z, air);
        block->onNeighbourChanged(owner, level, position, vein);
        const BlockState floorOnly = level.getBlockState(position.x, position.y, position.z);
        require(floorOnly.mName == BlockNames::SCULK_VEIN
                        && floorOnly.mStates.getInt("multi_face_direction_bits", 0)
                                   == PlacementOrientation::multiFaceBit(PlacementOrientation::FACE_DOWN),
                "the vein kept the face of the removed wall");

        level.setBlockState(floor.x, floor.y, floor.z, air);
        block->onNeighbourChanged(owner, level, position, floorOnly);
        require(level.getBlockState(position.x, position.y, position.z).mName == BlockNames::AIR,
                "a vein with no support left stayed");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-multiface-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        placement();
        support();
        std::cout << "multiface block checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
