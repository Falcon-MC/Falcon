#include "World/Level/LevelGen/Overworld/Feature/Decoration/ReedsFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const int32_t SEA_LEVEL = 63;

    const BlockState &reedsState() {
        static const BlockState state = DecorationSupport::withState(
                VanillaBlocks::REEDS().toBlockState(), "age", 15);
        return state;
    }

}

const char *ReedsFeature::name() const {
    return "minecraft:reeds_feature";
}

void ReedsFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ));
    if (mRandom.nextInt(20) != 0)
        return;

    int32_t maxReed = mRandom.nextInt(3);

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            if (chunk.getHeight(x, z) != SEA_LEVEL)
                continue;

            const int32_t worldX = x + (chunkX << 4);
            const int32_t worldZ = z + (chunkZ << 4);
            const BlockState support = chunk.getBlock(x, SEA_LEVEL, z);
            if (!DecorationSupport::reedsSupportValid(level, support, worldX, SEA_LEVEL, worldZ))
                continue;

            for (int32_t i = 1; i < 4; i++)
                chunk.setBlock(x, SEA_LEVEL + i, z, reedsState());

            if (--maxReed <= 0)
                return;
        }
    }
}
