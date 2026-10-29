#include "World/Level/LevelGen/Overworld/Feature/Decoration/UnderwaterMagmaFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>
#include <limits>

namespace {

    const int32_t FLOOR_SEARCH_RANGE = 5;

    const BlockState &magmaState() {
        static const BlockState state = VanillaBlocks::MAGMA().toBlockState();
        return state;
    }

}

const char *UnderwaterMagmaFeature::name() const {
    return "minecraft:overworld_underwater_magma_feature";
}

int32_t UnderwaterMagmaFeature::_findFloorY(LevelChunk &chunk, int32_t x, int32_t originY, int32_t z) {
    if (originY < LevelChunk::MIN_Y || originY > LevelChunk::MAX_Y + 1
        || !DecorationSupport::isWater(chunk.getBlock(x, originY, z)))
        return std::numeric_limits<int32_t>::min();

    const int32_t minY = std::max((int32_t) LevelChunk::MIN_Y, originY - FLOOR_SEARCH_RANGE);
    for (int32_t y = originY - 1; y >= minY; y--) {
        if (!DecorationSupport::isWater(chunk.getBlock(x, y, z)))
            return y;
    }

    return std::numeric_limits<int32_t>::min();
}

bool UnderwaterMagmaFeature::_isConnectedToSurfaceWater(LevelChunk &chunk, int32_t x, int32_t originY, int32_t z) {
    const int32_t surfaceY = chunk.getHeight(x, z);
    if (originY >= surfaceY)
        return true;

    for (int32_t y = originY + 1; y <= surfaceY; y++) {
        if (!DecorationSupport::isWater(chunk.getBlock(x, y, z)))
            return false;
    }

    return true;
}

bool UnderwaterMagmaFeature::_canPlaceMagma(LevelChunk &chunk, int32_t x, int32_t y, int32_t z) {
    const BlockState &target = chunk.getBlock(x, y, z);
    if (DecorationSupport::isWater(target) || DecorationSupport::isAir(target))
        return false;

    if (y - 1 < LevelChunk::MIN_Y || x - 1 < 0 || x + 1 > 15 || z - 1 < 0 || z + 1 > 15)
        return false;

    return DecorationSupport::isSolid(chunk.getBlock(x, y - 1, z))
           && DecorationSupport::isSolid(chunk.getBlock(x + 1, y, z))
           && DecorationSupport::isSolid(chunk.getBlock(x - 1, y, z))
           && DecorationSupport::isSolid(chunk.getBlock(x, y, z + 1))
           && DecorationSupport::isSolid(chunk.getBlock(x, y, z - 1));
}

void UnderwaterMagmaFeature::_placeMagmaBlob(LevelChunk &chunk, BlockManager &manager, int32_t floorX, int32_t floorY,
                                             int32_t floorZ) {
    const int32_t minY = LevelChunk::MIN_Y;
    const int32_t maxY = LevelChunk::MAX_Y + 1;

    for (int32_t x = floorX - 1; x <= floorX + 1; x++) {
        if (x < 0 || x > 15)
            continue;

        for (int32_t y = std::max(minY, floorY - 1); y <= std::min(maxY, floorY + 1); y++) {
            for (int32_t z = floorZ - 1; z <= floorZ + 1; z++) {
                if (z < 0 || z > 15 || mRandom.nextFloat() >= 0.5f || !_canPlaceMagma(chunk, x, y, z))
                    continue;

                const int32_t worldX = (chunk.getX() << 4) + x;
                const int32_t worldZ = (chunk.getZ() << 4) + z;
                manager.setBlockStateAt(worldX, y, worldZ, magmaState());
            }
        }
    }
}

void UnderwaterMagmaFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ) ^ javaStringHash(name()));

    BlockManager manager(level);
    const int32_t count = 44 + mRandom.nextInt(9);
    const int32_t minY = LevelChunk::MIN_Y;
    const int32_t maxY = std::min(256, (int32_t) LevelChunk::MAX_Y + 1);

    for (int32_t i = 0; i < count; i++) {
        const int32_t x = mRandom.nextInt(16);
        const int32_t z = mRandom.nextInt(16);
        const int32_t sampledY = minY + mRandom.nextInt(maxY - minY + 1);
        const int32_t floorY = _findFloorY(chunk, x, sampledY, z);

        if (floorY > minY && !_isConnectedToSurfaceWater(chunk, x, sampledY, z))
            _placeMagmaBlob(chunk, manager, x, floorY, z);
    }

    queueObject(manager);
}
