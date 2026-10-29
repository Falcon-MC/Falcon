#include "World/Level/LevelGen/Overworld/Feature/Decoration/LegacySmallMushroomsFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

namespace {

    const BlockState &redMushroomState() {
        static const BlockState state = VanillaBlocks::RED_MUSHROOM().toBlockState();
        return state;
    }

}

const char *LegacySmallMushroomsFeature::name() const {
    return "minecraft:scatter_red_mushroom_feature";
}

BlockState LegacySmallMushroomsFeature::getSourceBlock() const {
    return redMushroomState();
}

int32_t LegacySmallMushroomsFeature::getMinRadius() const {
    return 1;
}

int32_t LegacySmallMushroomsFeature::getMaxRadius() const {
    return 2;
}

double LegacySmallMushroomsFeature::getProbability() const {
    return (double) 0.1f;
}

int32_t LegacySmallMushroomsFeature::getBase() const {
    return -7;
}

int32_t LegacySmallMushroomsFeature::getRandom() const {
    return 8;
}

bool LegacySmallMushroomsFeature::isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y,
                                                 int32_t z) const {
    return DecorationSupport::isSolid(support)
           && (level.getHeightAt(x, z) != y
               || DecorationSupport::getBiomeAt(level, x, y, z) == BiomeIds::MUSHROOM_ISLAND);
}

int32_t LegacySmallMushroomsFeature::getY(LevelChunk &chunk, Level &level, int32_t x, int32_t z) const {
    const int32_t startY = DiscFeature::getY(chunk, level, x, z);
    const int32_t worldX = (chunk.getX() << 4) + x;
    const int32_t worldZ = (chunk.getZ() << 4) + z;

    for (int32_t y = startY; y > LevelChunk::MIN_Y; y--) {
        if (!DecorationSupport::isAir(chunk.getBlock(x, y + 1, z)))
            continue;

        if (isSupportValid(chunk.getBlock(x, y, z), level, worldX, y, worldZ))
            return y;
    }

    return startY;
}
