#include "World/Level/LevelGen/Nether/Feature/NetherSpringFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {
    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }
}

const char *NetherSpringFeature::name() const {
    return "nether_lava";
}

int32_t NetherSpringFeature::_getHighestWorkableBlock(BlockManager &manager, int32_t x, int32_t z) {
    int32_t y = 127;
    for (; y >= 0; y--) {
        if (isAir(manager.getBlockAt(x, y, z)))
            break;
    }

    return y == 0 ? -1 : y;
}

void NetherSpringFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    seedForChunk(level, chunkX, chunkZ);

    BlockManager object(level);
    const int32_t amount = mRandom.nextBoundedInt(30) - 29;

    for (int32_t i = 0; i < amount; i++) {
        const int32_t x = randomRange(mRandom, chunkX << 4, (chunkX << 4) + 15);
        const int32_t z = randomRange(mRandom, chunkZ << 4, (chunkZ << 4) + 15);
        const int32_t y = _getHighestWorkableBlock(object, x, z);
        if (y <= 1)
            continue;

        if (mRandom.nextInt(4) == 1)
            continue;

        object.setBlockStateAt(x, y + 1, z, lavaState());
    }

    queueObject(object);
}
