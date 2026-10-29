#include "World/Level/LevelGen/Overworld/Feature/Decoration/LakeFeature.h"

#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>

namespace {

    bool isNotFeatureReplaceable(const BlockState &state) {
        return state.mName == "minecraft:bedrock";
    }

}

int32_t LakeFeature::placementAttempts() const {
    return 1;
}

BlockState LakeFeature::barrier() const {
    return DecorationSupport::airState();
}

bool LakeFeature::canPlaceFeature(BlockManager &manager, int32_t x, int32_t y, int32_t z) const {
    (void) manager;
    (void) x;
    (void) y;
    (void) z;
    return true;
}

bool LakeFeature::canReplaceWithAirOrFluid(BlockManager &manager, int32_t x, int32_t y, int32_t z) const {
    return !isNotFeatureReplaceable(manager.getBlockAt(x, y, z));
}

bool LakeFeature::canReplaceWithBarrier(BlockManager &manager, int32_t x, int32_t y, int32_t z) const {
    return canReplaceWithAirOrFluid(manager, x, y, z);
}

int32_t LakeFeature::_index(int32_t x, int32_t y, int32_t z) {
    return (x * 16 + z) * 8 + y;
}

bool LakeFeature::_isBoundary(const bool *grid, int32_t xx, int32_t yy, int32_t zz) {
    return !grid[_index(xx, yy, zz)]
           && ((xx < 15 && grid[_index(xx + 1, yy, zz)])
               || (xx > 0 && grid[_index(xx - 1, yy, zz)])
               || (zz < 15 && grid[_index(xx, yy, zz + 1)])
               || (zz > 0 && grid[_index(xx, yy, zz - 1)])
               || (yy < 7 && grid[_index(xx, yy + 1, zz)])
               || (yy > 0 && grid[_index(xx, yy - 1, zz)]));
}

bool LakeFeature::_validateBoundary(BlockManager &manager, const bool *grid, int32_t originX, int32_t originY,
                                    int32_t originZ, const BlockState &fluidState) const {
    for (int32_t xx = 0; xx < 16; xx++) {
        for (int32_t zz = 0; zz < 16; zz++) {
            for (int32_t yy = 0; yy < 8; yy++) {
                if (!_isBoundary(grid, xx, yy, zz))
                    continue;

                const int32_t x = originX + xx;
                const int32_t y = originY + yy;
                const int32_t z = originZ + zz;
                const BlockState block = manager.getBlockAt(x, y, z);

                if (yy >= 4 && DecorationSupport::isLiquid(block))
                    return false;

                if (yy < 4 && !DecorationSupport::isSolid(block) && !(block == fluidState))
                    return false;

                if (!canPlaceFeature(manager, x, y, z))
                    return false;
            }
        }
    }

    return true;
}

void LakeFeature::_carveLake(BlockManager &manager, const bool *grid, int32_t originX, int32_t originY,
                             int32_t originZ, const BlockState &fluidState) {
    for (int32_t xx = 0; xx < 16; xx++) {
        for (int32_t zz = 0; zz < 16; zz++) {
            for (int32_t yy = 0; yy < 8; yy++) {
                if (!grid[_index(xx, yy, zz)])
                    continue;

                const int32_t x = originX + xx;
                const int32_t y = originY + yy;
                const int32_t z = originZ + zz;
                if (!canReplaceWithAirOrFluid(manager, x, y, z))
                    continue;

                manager.setBlockStateAt(x, y, z, yy >= 4 ? DecorationSupport::airState() : fluidState);
            }
        }
    }
}

void LakeFeature::_placeBarrier(BlockManager &manager, const bool *grid, int32_t originX, int32_t originY,
                                int32_t originZ) {
    const BlockState barrierState = barrier();
    if (DecorationSupport::isAir(barrierState))
        return;

    for (int32_t xx = 0; xx < 16; xx++) {
        for (int32_t zz = 0; zz < 16; zz++) {
            for (int32_t yy = 0; yy < 8; yy++) {
                if (!_isBoundary(grid, xx, yy, zz) || (yy >= 4 && mRandom.nextInt(2) == 0))
                    continue;

                const int32_t x = originX + xx;
                const int32_t y = originY + yy;
                const int32_t z = originZ + zz;
                if (DecorationSupport::isSolid(manager.getBlockAt(x, y, z)) && canReplaceWithBarrier(manager, x, y, z))
                    manager.setBlockStateAt(x, y, z, barrierState);
            }
        }
    }
}

bool LakeFeature::place(BlockManager &manager, int32_t originX, int32_t originY, int32_t originZ) {
    if (originY <= LevelChunk::MIN_Y + 4)
        return false;

    originX -= 8;
    originY -= 4;
    originZ -= 8;

    bool grid[16 * 16 * 8] = {false};
    const int32_t spots = mRandom.nextInt(4) + 4;

    for (int32_t i = 0; i < spots; i++) {
        const double xr = mRandom.nextDouble() * 6.0 + 3.0;
        const double yr = mRandom.nextDouble() * 4.0 + 2.0;
        const double zr = mRandom.nextDouble() * 6.0 + 3.0;
        const double xp = mRandom.nextDouble() * (16.0 - xr - 2.0) + 1.0 + xr / 2.0;
        const double yp = mRandom.nextDouble() * (8.0 - yr - 4.0) + 2.0 + yr / 2.0;
        const double zp = mRandom.nextDouble() * (16.0 - zr - 2.0) + 1.0 + zr / 2.0;

        for (int32_t xx = 1; xx < 15; xx++) {
            for (int32_t zz = 1; zz < 15; zz++) {
                for (int32_t yy = 1; yy < 7; yy++) {
                    const double xd = ((double) xx - xp) / (xr / 2.0);
                    const double yd = ((double) yy - yp) / (yr / 2.0);
                    const double zd = ((double) zz - zp) / (zr / 2.0);
                    if (xd * xd + yd * yd + zd * zd < 1.0)
                        grid[_index(xx, yy, zz)] = true;
                }
            }
        }
    }

    const BlockState fluidState = fluid();
    if (!_validateBoundary(manager, grid, originX, originY, originZ, fluidState))
        return false;

    _carveLake(manager, grid, originX, originY, originZ, fluidState);
    _placeBarrier(manager, grid, originX, originY, originZ);
    return true;
}

void LakeFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ) ^ javaStringHash(name()));

    BlockManager manager(level);
    const int32_t minY = LevelChunk::MIN_Y + 5;
    const int32_t maxY = std::max(minY, chunk.getHeight(mRandom.nextInt(16), mRandom.nextInt(16)));

    for (int32_t i = 0; i < placementAttempts(); i++) {
        const int32_t x = (chunkX << 4) + mRandom.nextInt(16);
        const int32_t y = minY + mRandom.nextInt(maxY - minY + 1);
        const int32_t z = (chunkZ << 4) + mRandom.nextInt(16);
        if (place(manager, x, y, z)) {
            queueObject(manager);
            return;
        }
    }
}
