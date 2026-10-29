#include "World/Level/LevelGen/Overworld/Feature/Decoration/VegetationFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const BlockState &shortGrassState() {
        static const BlockState state = VanillaBlocks::SHORT_GRASS().toBlockState();
        return state;
    }

}

const char *VegetationFeature::name() const {
    return "minecraft:scatter_tall_grass_feature";
}

int32_t VegetationFeature::getBase() const {
    return 10;
}

int32_t VegetationFeature::getRandom() const {
    return 0;
}

void VegetationFeature::place(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    manager.setBlockStateAt(x, y, z, shortGrassState());
}

void VegetationFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    const int32_t centerHeight = level.getHeightAt((chunkX << 4) + 7, (chunkZ << 4) + 7);
    const int32_t centerBiome = (int32_t) chunk.getBiomeAt(7, centerHeight, 7);
    if (DecorationSupport::isBiomeMooshroomIsland(centerBiome))
        return;

    const int32_t x = random.nextBoundedInt(15);
    const int32_t z = random.nextBoundedInt(15);
    int32_t y = chunk.getHeight(x, z);
    const int32_t worldX = (chunkX << 4) + x;
    const int32_t worldZ = (chunkZ << 4) + z;

    while (!isSupportValid(chunk.getBlock(x, y, z), level, worldX, y, worldZ) && y > SEA_LEVEL)
        y--;

    if (y < SEA_LEVEL || !isSupportValid(chunk.getBlock(x, y, z), level, worldX, y, worldZ))
        return;

    if (!DecorationSupport::isAir(chunk.getBlock(x, y + 1, z)))
        return;

    BlockManager object(level);
    place(object, worldX, y + 1, worldZ);
    queueObject(object);
}
