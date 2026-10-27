#include "World/Level/LevelGen/Overworld/Feature/Decoration/SpringFeature.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"
#include "World/Level/Chunk/LevelChunk.h"

#include <algorithm>

namespace {

    const BlockState &waterState() {
        static const BlockState state = VanillaBlocks::WATER().toBlockState();
        return state;
    }

    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }

    bool isValidBlockId(const std::string &id) {
        return id == "minecraft:stone"
               || id == "minecraft:granite"
               || id == "minecraft:diorite"
               || id == "minecraft:andesite"
               || id == "minecraft:deepslate"
               || id == "minecraft:tuff"
               || id == "minecraft:calcite"
               || id == "minecraft:dirt"
               || id == "minecraft:grass_block"
               || id == "minecraft:powder_snow";
    }

    const DecorationSupport::FaceOffset SPRING_NEIGHBORS[5] = {
            {0,  -1, 0},
            {0,  0,  -1},
            {0,  0,  1},
            {-1, 0,  0},
            {1,  0,  0}
    };

}

const char *SpringFeature::name() const {
    return "minecraft:overworld_surface_springs_feature";
}

bool SpringFeature::_canPlaceSpring(BlockManager &manager, int32_t x, int32_t y, int32_t z) {
    if (!isValidBlockId(manager.getBlockAt(x, y + 1, z).mName)
        || !isValidBlockId(manager.getBlockAt(x, y - 1, z).mName))
        return false;

    const BlockState current = manager.getBlockAt(x, y, z);
    if (!DecorationSupport::isAir(current) && !isValidBlockId(current.mName))
        return false;

    int32_t rockCount = 0;
    int32_t holeCount = 0;

    for (const DecorationSupport::FaceOffset &face: SPRING_NEIGHBORS) {
        const BlockState state = manager.getBlockAt(x + face.mX, y + face.mY, z + face.mZ);
        if (isValidBlockId(state.mName))
            rockCount++;

        if (DecorationSupport::isAir(state))
            holeCount++;
    }

    return rockCount == 4 && holeCount == 1;
}

int32_t SpringFeature::_nextUniformY(int32_t minY, int32_t maxY) {
    return minY + mRandom.nextInt(maxY - minY + 1);
}

int32_t SpringFeature::_nextVeryBiasedToBottomY(int32_t minY, int32_t maxY) {
    if (maxY - minY - 8 + 1 <= 0)
        return minY;

    const int32_t upperInclusive = randomRange(mRandom, minY + 8, maxY);
    const int32_t biasedUpperInclusive = randomRange(mRandom, minY, upperInclusive - 1);
    return randomRange(mRandom, minY, biasedUpperInclusive - 1 + 8);
}

void SpringFeature::_placeSprings(BlockManager &manager, int32_t chunkX, int32_t chunkZ, int32_t count, int32_t minY,
                                  int32_t maxY, const BlockState &fluid, bool lava) {
    if (maxY < minY)
        return;

    const int32_t sourceX = chunkX << 4;
    const int32_t sourceZ = chunkZ << 4;

    for (int32_t i = 0; i < count; i++) {
        const int32_t x = sourceX + mRandom.nextInt(14) + 1;
        const int32_t y = lava ? _nextVeryBiasedToBottomY(minY, maxY) : _nextUniformY(minY, maxY);
        const int32_t z = sourceZ + mRandom.nextInt(14) + 1;

        if (_canPlaceSpring(manager, x, y, z))
            manager.setBlockStateAt(x, y, z, fluid);
    }
}

void SpringFeature::apply(ChunkGenerateContext &context) {
    LevelChunk &chunk = context.getChunk();
    Level &level = context.getLevel();
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    mRandom.setSeed(level.getSeed() ^ DecorationSupport::chunkHash(chunkX, chunkZ) ^ javaStringHash(name()));

    BlockManager manager(level);
    const int32_t minY = LevelChunk::MIN_Y;
    const int32_t maxHeight = LevelChunk::MAX_Y + 1;

    _placeSprings(manager, chunkX, chunkZ, 25, minY, std::min(192, maxHeight), waterState(), false);
    _placeSprings(manager, chunkX, chunkZ, 20, minY, std::max(minY, maxHeight - 8), lavaState(), true);

    queueObject(manager);
}
