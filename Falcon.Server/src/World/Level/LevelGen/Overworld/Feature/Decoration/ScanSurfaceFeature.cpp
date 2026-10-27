#include "World/Level/LevelGen/Overworld/Feature/Decoration/ScanSurfaceFeature.h"

#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

bool ScanSurfaceFeature::isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y, int32_t z) const {
    (void) level;
    (void) x;
    (void) y;
    (void) z;

    return IFeature::isSupportDirt(support) && support.mName != "minecraft:coarse_dirt";
}

void ScanSurfaceFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    const int32_t x = random.nextBoundedInt(15);
    const int32_t z = random.nextBoundedInt(15);
    int32_t y = chunk.getHeight(x, z);
    const int32_t worldX = (chunkX << 4) + x;
    const int32_t worldZ = (chunkZ << 4) + z;

    while (!isSupportValid(chunk.getBlock(x, y, z), level, worldX, y, worldZ) && y >= SEA_LEVEL - 1)
        y--;

    if (y < SEA_LEVEL - 1 || !isSupportValid(chunk.getBlock(x, y, z), level, worldX, y, worldZ))
        return;

    if (!DecorationSupport::isAir(chunk.getBlock(x, y + 1, z)))
        return;

    BlockManager object(level);
    place(object, worldX, y + 1, worldZ);
    queueObject(object);
}
