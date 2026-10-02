#include "Level/Generator/Overworld/Feature/Decoration/IceCrystalScatterFeature.h"

#include "Level/Generator/Feature/BlockManager.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Generator/Overworld/Feature/Decoration/IceCavesSupport.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"

namespace {
    const int32_t MIN_ATTEMPTS = 48;
    const int32_t EXTRA_ATTEMPTS = 16;
    const int32_t SCATTER_COUNT = 2;
    const int32_t SCATTER_RADIUS = 2;
    const int32_t SEARCH_RANGE = 32;
    const int32_t MIN_Y = -64;
    const int32_t HEIGHT_RANGE = 320;
}

const char *IceCrystalScatterFeature::name() const {
    return "minecraft:ice_crystal_scatter_feature";
}

void IceCrystalScatterFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunk.getX(), chunk.getZ())
                    ^ javaStringHash(name()));

    BlockManager manager(level);
    manager.readPendingFrom(mRoot);
    const int32_t attempts = MIN_ATTEMPTS + mRandom.nextBoundedInt(EXTRA_ATTEMPTS);
    for (int32_t i = 0; i < attempts; i++) {
        const int32_t baseX = (chunk.getX() << 4) + mRandom.nextBoundedInt(15);
        const int32_t baseY = MIN_Y + mRandom.nextBoundedInt(HEIGHT_RANGE);
        const int32_t baseZ = (chunk.getZ() << 4) + mRandom.nextBoundedInt(15);

        for (int32_t j = 0; j < SCATTER_COUNT; j++) {
            const int32_t z = baseZ + IceCavesSupport::triangle(mRandom, -SCATTER_RADIUS, SCATTER_RADIUS);
            const int32_t x = baseX + IceCavesSupport::triangle(mRandom, -SCATTER_RADIUS, SCATTER_RADIUS);
            if (!IceCavesSupport::isIceCaves(chunk, x, baseY, z))
                continue;

            int32_t floorY = 0;
            if (!IceCavesSupport::findFloor(manager, x, baseY, z, SEARCH_RANGE, floorY))
                continue;

            if (manager.getBlockAt(x, floorY, z).getHash() != IceCavesSupport::packedIceState().getHash())
                continue;

            manager.setBlockStateAt(x, floorY + 1, z, IceCavesSupport::iceCrystalState());
        }
    }

    queueObject(manager);
}
