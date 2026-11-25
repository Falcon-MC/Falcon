#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/SculkVeinBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void placement() {
        const auto *vein = dynamic_cast<const SculkVeinBlock *>(VanillaBlocks::fromIdentifier(BlockNames::SCULK_VEIN));
        require(vein != nullptr, "sculk vein has no block class");

        Level level("sculk-vein-test", 2);
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i position(8, 20, 8);
        const BlockState stone = VanillaBlocks::STONE().toBlockState();
        const BlockState shortGrass = VanillaBlocks::SHORT_GRASS().toBlockState();
        const BlockState air = VanillaBlocks::AIR().toBlockState();

        for (int face = 0; face < 6; ++face) {
            const Vector3i support = PlacementOrientation::relativePosition(position, face ^ 1);
            level.setBlock(support, stone, false);
            require(vein->canPlaceAt(level, position, face), "vein rejected a full face");
            level.setBlock(support, shortGrass, false);
            require(!vein->canPlaceAt(level, position, face), "vein clung to short grass");
            level.setBlock(support, air, false);
            require(!vein->canPlaceAt(level, position, face), "vein clung to air");
        }
    }
}

int main() {
    try {
        placement();
        std::cout << "sculk vein checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
