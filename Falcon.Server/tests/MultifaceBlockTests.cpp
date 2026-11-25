#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/MultifaceBlock.h"
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
}

int main() {
    try {
        placement();
        std::cout << "multiface block checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
