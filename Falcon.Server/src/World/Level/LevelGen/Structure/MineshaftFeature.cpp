#include "World/Level/LevelGen/Structure/MineshaftFeature.h"

#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Random/GameRandom.h"

#include <algorithm>
#include <cstdlib>

namespace {
    const int32_t SEARCH_RADIUS = 100;
    const MineshaftFeature gMineshaft;
}

MineshaftFeature::MineshaftFeature() : StructureFeature("mineshaft", 1, 0, 0, Spread::Linear) {}

bool MineshaftFeature::rollStart(int64_t seed, int32_t chunkX, int32_t chunkZ, std::mt19937 &random) {
    const uint32_t worldSeed = (uint32_t) seed;
    random.seed(worldSeed);
    const uint32_t xMultiplier = (uint32_t) GameRandom::nextInt(random);
    const uint32_t zMultiplier = (uint32_t) GameRandom::nextInt(random);
    random.seed(worldSeed ^ (xMultiplier * (uint32_t) chunkX) ^ (zMultiplier * (uint32_t) chunkZ));
    random.discard(1);
    const float chance = GameRandom::nextFloat(random);
    const int distanceDraw = GameRandom::nextInt(random, 80);
    const int64_t distance = std::max(std::abs((int64_t) chunkX), std::abs((int64_t) chunkZ));
    return chance < 0.004f && distanceDraw < distance;
}

bool MineshaftFeature::_isValidStart(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    std::mt19937 random;
    return rollStart(generator.getSeed(), chunkX, chunkZ, random);
}

bool MineshaftFeature::isStartChunk(const ChunkGenerator &generator, int32_t chunkX, int32_t chunkZ) const {
    return _isValidStart(generator, chunkX, chunkZ);
}

bool MineshaftFeature::findNearest(const ChunkGenerator &generator, int32_t blockX, int32_t blockZ,
                                   StructureChunkPos &out) const {
    const int32_t originX = blockX >> 4;
    const int32_t originZ = blockZ >> 4;
    for (int32_t ring = 0; ring <= SEARCH_RADIUS; ++ring) {
        for (int32_t dx = -ring; dx <= ring; ++dx) {
            const int32_t step = std::abs(dx) == ring || ring == 0 ? 1 : 2 * ring;
            for (int32_t dz = -ring; dz <= ring; dz += step) {
                if (isStartChunk(generator, originX + dx, originZ + dz)) {
                    out = {originX + dx, originZ + dz};
                    return true;
                }
            }
        }
    }
    return false;
}
