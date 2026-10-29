#include "World/Level/LevelGen/Overworld/Feature/Decoration/PaleMossPatchFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const int32_t SEA_LEVEL = 63;

    const BlockState &grassBlockState() {
        static const BlockState state = VanillaBlocks::GRASS_BLOCK().toBlockState();
        return state;
    }

    const BlockState &paleMossBlockState() {
        static const BlockState state = VanillaBlocks::PALE_MOSS_BLOCK().toBlockState();
        return state;
    }

    const BlockState &paleMossCarpetState() {
        static const BlockState state = VanillaBlocks::PALE_MOSS_CARPET().toBlockState();
        return state;
    }

}

const char *PaleMossPatchFeature::name() const {
    return "minecraft:pale_moss_patch_feature";
}

void PaleMossPatchFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ));

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            if (!DecorationSupport::isBiomePaleGarden((int32_t) chunk.getBiomeAt(x, SEA_LEVEL, z)))
                continue;

            if (mRandom.nextInt(9) >= 7)
                continue;

            int32_t y = chunk.getHeight(x, z);
            while (!(chunk.getBlock(x, y, z) == grassBlockState()) && y > SEA_LEVEL)
                y--;

            if (y <= SEA_LEVEL)
                continue;

            chunk.setBlock(x, y, z, paleMossBlockState());
            if (mRandom.nextInt(9) < 2)
                chunk.setBlock(x, y + 1, z, paleMossCarpetState());
        }
    }
}
