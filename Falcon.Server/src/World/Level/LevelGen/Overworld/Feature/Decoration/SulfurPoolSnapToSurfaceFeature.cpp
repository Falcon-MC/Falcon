#include "World/Level/LevelGen/Overworld/Feature/Decoration/SulfurPoolSnapToSurfaceFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <limits>

namespace {

    const int32_t PLACEMENT_ATTEMPTS = 256;
    const int32_t MAX_ENVIRONMENT_SCAN_STEPS = 32;

    const BlockState &waterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    const BlockState &sulfurState() {
        static const BlockState state = VanillaBlocks::SULFUR().toBlockState();
        return state;
    }

    const BlockState &wetPotentSulfurState() {
        static const BlockState state = DecorationSupport::withState(
                VanillaBlocks::POTENT_SULFUR().toBlockState(), "potent_sulfur_state", "wet");
        return state;
    }

}

const char *SulfurPoolSnapToSurfaceFeature::name() const {
    return "minecraft:sulfur_pool_with_potent_sulfur_snap_to_surface_feature";
}

BlockState SulfurPoolSnapToSurfaceFeature::fluid() const {
    return waterState();
}

BlockState SulfurPoolSnapToSurfaceFeature::barrier() const {
    return sulfurState();
}

bool SulfurPoolSnapToSurfaceFeature::canPlaceFeature(BlockManager &manager, int32_t x, int32_t y, int32_t z) const {
    return manager.getBlockAt(x, y, z).mName != "minecraft:sulfur_spike";
}

void SulfurPoolSnapToSurfaceFeature::_placePotentSulfur(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    for (int32_t step = 0; step <= 4; step++) {
        const int32_t scanY = y - step;
        const BlockState block = manager.getBlockAt(x, scanY, z);
        if (DecorationSupport::isSolid(block)
            && DecorationSupport::isWater(manager.getBlockAt(x, scanY + 1, z))) {
            manager.setBlockStateAt(x, scanY, z, wetPotentSulfurState());
            return;
        }
    }
}

int32_t SulfurPoolSnapToSurfaceFeature::_scanUpToSurface(BlockManager &manager, int32_t x, int32_t y, int32_t z,
                                                         int32_t maxY) {
    for (int32_t step = 0; step <= MAX_ENVIRONMENT_SCAN_STEPS && y + step <= maxY; step++) {
        const int32_t scanY = y + step;
        const BlockState block = manager.getBlockAt(x, scanY, z);
        if (DecorationSupport::isAir(block) || block.mName == "minecraft:sulfur_spike")
            return scanY - 1;
    }

    return std::numeric_limits<int32_t>::min();
}

void SulfurPoolSnapToSurfaceFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ) ^ javaStringHash(name()));

    BlockManager manager(level);
    const int32_t minY = LevelChunk::MIN_Y;
    const int32_t maxY = std::min(256, (int32_t) LevelChunk::MAX_Y + 1);
    bool placed = false;

    for (int32_t i = 0; i < PLACEMENT_ATTEMPTS; i++) {
        const int32_t x = (chunkX << 4) + mRandom.nextInt(16);
        const int32_t y = minY + mRandom.nextInt(maxY - minY + 1);
        const int32_t z = (chunkZ << 4) + mRandom.nextInt(16);

        if (!DecorationSupport::isSolid(manager.getBlockAt(x, y, z)))
            continue;

        const int32_t surfaceY = _scanUpToSurface(manager, x, y, z, maxY);
        if (surfaceY == std::numeric_limits<int32_t>::min()
            || DecorationSupport::getBiomeAt(level, x, surfaceY, z) != BiomeIds::SULFUR_CAVES)
            continue;

        const int32_t lakeY = surfaceY + 1;
        if (place(manager, x, lakeY, z)) {
            _placePotentSulfur(manager, x, lakeY, z);
            placed = true;
            break;
        }
    }

    if (placed)
        queueObject(manager);
}
