#include "Level/Generator/Overworld/Feature/Decoration/LargeIcicleFeature.h"

#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Generator/Overworld/Feature/Decoration/IceCavesSupport.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

namespace {
    const int32_t MIN_ATTEMPTS = 48;
    const int32_t EXTRA_ATTEMPTS = 16;
    const int32_t SEARCH_RANGE = 12;
    const int32_t MIN_LENGTH = 3;
    const int32_t EXTRA_LENGTH = 4;
    const int32_t MIN_Y = -64;
    const int32_t HEIGHT_RANGE = 320;
}

const char *LargeIcicleFeature::name() const {
    return "minecraft:large_icicle_feature";
}

void LargeIcicleFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunk.getX(), chunk.getZ())
                    ^ javaStringHash(name()));

    BlockManager manager(level);
    const int32_t attempts = MIN_ATTEMPTS + mRandom.nextBoundedInt(EXTRA_ATTEMPTS);
    for (int32_t i = 0; i < attempts; i++) {
        const int32_t x = (chunk.getX() << 4) + mRandom.nextBoundedInt(15);
        const int32_t y = MIN_Y + mRandom.nextBoundedInt(HEIGHT_RANGE);
        const int32_t z = (chunk.getZ() << 4) + mRandom.nextBoundedInt(15);
        if (!IceCavesSupport::isIceCaves(chunk, x, y, z))
            continue;

        int32_t ceilingY = 0;
        if (IceCavesSupport::findCeiling(manager, x, y, z, SEARCH_RANGE, ceilingY))
            IceCavesSupport::placeIcicle(manager, x, ceilingY, z, MIN_LENGTH + mRandom.nextBoundedInt(EXTRA_LENGTH));
    }

    queueObject(manager);
}
