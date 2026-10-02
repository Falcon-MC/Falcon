#include "Level/Generator/Overworld/Feature/Decoration/IcicleClusterFeature.h"

#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Generator/Overworld/Feature/Decoration/IceCavesSupport.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

namespace {
    const int32_t MIN_ATTEMPTS = 24;
    const int32_t EXTRA_ATTEMPTS = 40;
    const int32_t MIN_CLUSTER_SIZE = 2;
    const int32_t EXTRA_CLUSTER_SIZE = 3;
    const int32_t CLUSTER_RADIUS = 3;
    const int32_t SEARCH_RANGE = 12;
    const int32_t MAX_LENGTH = 3;
    const int32_t MIN_Y = -64;
    const int32_t HEIGHT_RANGE = 320;
}

const char *IcicleClusterFeature::name() const {
    return "minecraft:icicle_cluster_feature";
}

void IcicleClusterFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunk.getX(), chunk.getZ())
                    ^ javaStringHash(name()));

    BlockManager manager(level);
    const int32_t attempts = MIN_ATTEMPTS + mRandom.nextBoundedInt(EXTRA_ATTEMPTS);
    for (int32_t i = 0; i < attempts; i++) {
        const int32_t baseX = (chunk.getX() << 4) + mRandom.nextBoundedInt(15);
        const int32_t baseY = MIN_Y + mRandom.nextBoundedInt(HEIGHT_RANGE);
        const int32_t baseZ = (chunk.getZ() << 4) + mRandom.nextBoundedInt(15);
        const int32_t clusterSize = MIN_CLUSTER_SIZE + mRandom.nextBoundedInt(EXTRA_CLUSTER_SIZE);

        for (int32_t j = 0; j < clusterSize; j++) {
            const int32_t x = baseX + IceCavesSupport::triangle(mRandom, -CLUSTER_RADIUS, CLUSTER_RADIUS);
            const int32_t z = baseZ + IceCavesSupport::triangle(mRandom, -CLUSTER_RADIUS, CLUSTER_RADIUS);
            if (!IceCavesSupport::isIceCaves(chunk, x, baseY, z))
                continue;

            int32_t ceilingY = 0;
            if (IceCavesSupport::findCeiling(manager, x, baseY, z, SEARCH_RANGE, ceilingY))
                IceCavesSupport::placeIcicle(manager, x, ceilingY, z, 1 + mRandom.nextBoundedInt(MAX_LENGTH - 1));
        }
    }

    queueObject(manager);
}
