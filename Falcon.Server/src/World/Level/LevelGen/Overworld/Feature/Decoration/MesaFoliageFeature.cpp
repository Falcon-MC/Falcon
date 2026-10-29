#include "World/Level/LevelGen/Overworld/Feature/Decoration/MesaFoliageFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const char *CARDINAL_DIRECTIONS[4] = {"south", "west", "north", "east"};

    const BlockState &leafLitterState() {
        static const BlockState state = VanillaBlocks::LEAF_LITTER().toBlockState();
        return state;
    }

}

const char *MesaFoliageFeature::name() const {
    return "minecraft:mesa_foliage_feature";
}

void MesaFoliageFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ));

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            if (mRandom.nextInt(10) >= 2)
                continue;

            const int32_t y = chunk.getHeight(x, z) + 1;
            if (!DecorationSupport::isBiomeStone((int32_t) chunk.getBiomeAt(x, y, z)))
                continue;

            if (!isSupportDirt(chunk.getBlock(x, y - 1, z)))
                continue;

            if (!DecorationSupport::isAir(chunk.getBlock(x, y, z)))
                continue;

            BlockState state = DecorationSupport::withState(leafLitterState(), "minecraft:cardinal_direction",
                                                            CARDINAL_DIRECTIONS[mRandom.nextInt(3)]);
            state = DecorationSupport::withState(state, "growth", mRandom.nextInt(3));
            chunk.setBlock(x, y, z, state);
        }
    }
}
