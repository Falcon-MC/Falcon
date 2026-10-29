#include "World/Level/LevelGen/Nether/Feature/NyliumVegetationFeature.h"

#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

std::vector<int32_t> NyliumVegetationFeature::_getHighestWorkableBlocks(BlockManager &manager, int32_t x,
                                                                       int32_t z) const {
    std::vector<int32_t> blockYs;

    for (int32_t y = 128; y > 0; y--) {
        if (isName(manager.getBlockAt(x, y, z), getNyliumIdentifier())
            && isAir(manager.getBlockAt(x, y + 1, z)))
            blockYs.push_back(y + 1);
    }

    return blockYs;
}

void NyliumVegetationFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    seedForChunk(level, chunkX, chunkZ);

    BlockManager object(level);
    const int32_t amount = mRandom.nextInt(128) + 192;

    for (int32_t i = 0; i < amount; i++) {
        const int32_t x = randomRange(mRandom, chunkX << 4, (chunkX << 4) + 15);
        const int32_t z = randomRange(mRandom, chunkZ << 4, (chunkZ << 4) + 15);
        if (getBiomeIdAt(level, x, z) != getBiomeId())
            continue;

        const std::vector<int32_t> blockYs = _getHighestWorkableBlocks(object, x, z);
        for (int32_t y: blockYs) {
            if (y <= 1)
                continue;

            const int32_t randomType = mRandom.nextInt(6);
            object.setBlockStateAt(x, y, z, randomType == 0 ? getFungusState() : getRootsState());
        }
    }

    queueObject(object);
}
