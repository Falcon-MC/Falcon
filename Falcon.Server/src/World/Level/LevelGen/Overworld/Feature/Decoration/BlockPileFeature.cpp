#include "World/Level/LevelGen/Overworld/Feature/Decoration/BlockPileFeature.h"

#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/LevelGen/Overworld/Feature/FeatureRandom.h"

namespace {
    using FeatureRandom::nextFloat;
    using FeatureRandom::nextInt;

    // hay stands along y or x, one ice in seven is blue, one pumpkin in twenty carved
    BlockState pileBlock(BlockPileFeature::Pile pile, std::mt19937 &random) {
        switch (pile) {
            case BlockPileFeature::Pile::Hay: {
                const BlockState hay = VanillaBlocks::HAY_BLOCK().toBlockState();
                Tag states = hay.mStates;
                states.putString("pillar_axis", nextInt(random, 2) == 0 ? "y" : "x");
                return BlockState(hay.mName, states);
            }
            case BlockPileFeature::Pile::Melon:
                return VanillaBlocks::MELON_BLOCK().toBlockState();
            case BlockPileFeature::Pile::Pumpkin:
                return nextFloat(random) >= 0.95f ? VanillaBlocks::CARVED_PUMPKIN().toBlockState()
                                                  : VanillaBlocks::PUMPKIN().toBlockState();
            case BlockPileFeature::Pile::Snow:
                return VanillaBlocks::SNOW().toBlockState();
            default:
                return nextInt(random, 7) == 0 ? VanillaBlocks::BLUE_ICE().toBlockState()
                                               : VanillaBlocks::PACKED_ICE().toBlockState();
        }
    }

    // on a solid top or a path; only into air, the block drawn after both checks
    void tryPlace(BlockManager &blocks, BlockPileFeature::Pile pile, int32_t x, int32_t y, int32_t z,
                  std::mt19937 &random) {
        const BlockState &below = blocks.getBlockAt(x, y - 1, z);
        if (!BlockSupport::isSolid(below) && below.mName != "minecraft:grass_path")
            return;
        if (!DecorationSupport::isAir(blocks.getBlockAt(x, y, z)))
            return;

        blocks.setBlockStateAt(x, y, z, pileBlock(pile, random));
    }
}

bool BlockPileFeature::placeHay(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    return _place(blocks, Pile::Hay, x, y, z, random);
}

bool BlockPileFeature::placeMelon(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    return _place(blocks, Pile::Melon, x, y, z, random);
}

bool BlockPileFeature::placePumpkin(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    return _place(blocks, Pile::Pumpkin, x, y, z, random);
}

bool BlockPileFeature::placeSnow(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    return _place(blocks, Pile::Snow, x, y, z, random);
}

bool BlockPileFeature::placeIce(BlockManager &blocks, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    return _place(blocks, Pile::Ice, x, y, z, random);
}

bool BlockPileFeature::_place(BlockManager &blocks, Pile pile, int32_t x, int32_t y, int32_t z, std::mt19937 &random) {
    // x outermost, then the two layers, then z; a cell is filled inside a drawn distance or 3.1 % of
    // the time
    if (y < LevelChunk::MIN_Y + 5)
        return false;

    const int32_t radiusX = 2 + nextInt(random, 2);
    const int32_t radiusZ = 2 + nextInt(random, 2);
    for (int32_t atX = x - radiusX; atX <= x + radiusX; atX++) {
        for (int32_t atY = y; atY <= y + 1; atY++) {
            for (int32_t atZ = z - radiusZ; atZ <= z + radiusZ; atZ++) {
                const int32_t dx = x - atX;
                const int32_t dz = z - atZ;
                const float first = nextFloat(random);
                const float second = nextFloat(random);
                if (second * -6.0f + first * 10.0f < static_cast<float>(dx * dx + dz * dz)
                    && nextFloat(random) >= 0.031f)
                    continue;

                tryPlace(blocks, pile, atX, atY, atZ, random);
            }
        }
    }
    return true;
}
