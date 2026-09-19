#include "Level/Generator/Overworld/Feature/Decoration/SeaPickleFeature.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/LevelChunk.h"

namespace {

    const BlockState &seaPickleState() {
        static const BlockState state = VanillaBlocks::SEA_PICKLE().toBlockState();
        return state;
    }

}

const char *SeaPickleFeature::name() const {
    return "minecraft:pickle_feature";
}

int32_t SeaPickleFeature::getBase() const {
    return 1;
}

int32_t SeaPickleFeature::getRandom() const {
    return 0;
}

int32_t SeaPickleFeature::_findOceanFloorWaterY(LevelChunk &chunk, int32_t x, int32_t z) {
    int32_t y = chunk.getHeight(x, z);
    if (y <= LevelChunk::MIN_Y)
        return y;

    while (y > LevelChunk::MIN_Y && DecorationSupport::isWater(chunk.getBlock(x, y, z)))
        y--;

    return y + 1;
}

void SeaPickleFeature::populate(ChunkGenerateContext &context, IRandom &random) {
    LevelChunk &chunk = context.getChunk();

    const int32_t originX = random.nextInt(15);
    const int32_t originZ = random.nextInt(15);
    int32_t placed = 0;
    const int32_t tries = 20;

    for (int32_t i = 0; i < tries; i++) {
        const int32_t x = originX + random.nextInt(8) - random.nextInt(8);
        const int32_t z = originZ + random.nextInt(8) - random.nextInt(8);
        if (x < 0 || x > 15 || z < 0 || z > 15)
            continue;

        const int32_t y = _findOceanFloorWaterY(chunk, x, z);
        if (y <= LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
            continue;

        if (!DecorationSupport::isWater(chunk.getBlock(x, y, z))
            || !DecorationSupport::isSolid(chunk.getBlock(x, y - 1, z)))
            continue;

        const BlockState state = DecorationSupport::withState(seaPickleState(), "cluster_count", random.nextInt(4));
        DecorationSupport::setBlockKeepingWater(chunk, x, y, z, state);
        placed++;
    }

    if (placed != 0)
        return;

    const int32_t y = _findOceanFloorWaterY(chunk, originX, originZ);
    if (y > LevelChunk::MIN_Y && y <= LevelChunk::MAX_Y
        && DecorationSupport::isWater(chunk.getBlock(originX, y, originZ))
        && DecorationSupport::isSolid(chunk.getBlock(originX, y - 1, originZ))) {
        const BlockState state = DecorationSupport::withState(seaPickleState(), "cluster_count", random.nextInt(4));
        DecorationSupport::setBlockKeepingWater(chunk, originX, y, originZ, state);
    }
}
