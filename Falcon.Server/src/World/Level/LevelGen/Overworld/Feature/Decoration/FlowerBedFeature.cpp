#include "World/Level/LevelGen/Overworld/Feature/Decoration/FlowerBedFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const char *CARDINAL_DIRECTIONS[4] = {"south", "west", "north", "east"};

    const BlockState &pinkPetalsState() {
        static const BlockState state = VanillaBlocks::PINK_PETALS().toBlockState();
        return state;
    }

}

const char *FlowerBedFeature::name() const {
    return "minecraft:cherry_grove_surface_pink_petals_feature_rules";
}

void FlowerBedFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ));

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            if (!mRandom.nextBoolean())
                continue;

            const int32_t y = chunk.getHeight(x, z) + 1;
            if (!DecorationSupport::isBiomeCherryGrove((int32_t) chunk.getBiomeAt(x, y, z)))
                continue;

            if (!isSupportGrass(chunk.getBlock(x, y - 1, z)))
                continue;

            if (!DecorationSupport::isAir(chunk.getBlock(x, y, z)))
                continue;

            BlockState state = DecorationSupport::withState(pinkPetalsState(), "minecraft:cardinal_direction",
                                                            CARDINAL_DIRECTIONS[mRandom.nextInt(3)]);
            state = DecorationSupport::withState(state, "growth", mRandom.nextInt(3));
            chunk.setBlock(x, y, z, state);
        }
    }
}
