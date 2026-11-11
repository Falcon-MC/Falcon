#include "World/Level/LevelGen/End/TheEndGenerator.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/End/Biome/TheEndBiomeSource.h"
#include "World/Level/LevelGen/End/Feature/TheEndFeatureBuilder.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Protocol/BlockStateHasher.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {

    const BlockState &endStoneState() {
        static const BlockState state = VanillaBlocks::END_STONE().toBlockState();
        return state;
    }

    float clampFloat(float value, float min, float max) {
        if (value < min)
            return min;

        if (value > max)
            return max;

        return value;
    }

}

TheEndGenerator::TheEndGenerator(int64_t seed)
        : mSeed(seed), mAirHash(BlockStateHasher::hash("minecraft:air")), mNoiseHolder(), mBiomeSource() {
    mNoiseHolder.reset(new TheEndNoiseHolder(seed));
    mBiomeSource.reset(new TheEndBiomeSource());
}

TheEndGenerator::~TheEndGenerator() = default;

int64_t TheEndGenerator::chunkHash(int32_t chunkX, int32_t chunkZ) {
    return (int64_t) (((uint64_t) (uint32_t) chunkX << 32) | (uint64_t) (uint32_t) chunkZ);
}

int32_t TheEndGenerator::pickBiome(int32_t x, int32_t y, int32_t z) const {
    return mBiomeSource->pickBiomeId(x, y, z);
}

float TheEndGenerator::getIslandHeight(int32_t chunkX, int32_t chunkZ, int32_t x, int32_t z,
                                       const TheEndSimplexNoise &islandNoise) {
    const float centerX = (float) (chunkX * 2 + x);
    const float centerZ = (float) (chunkZ * 2 + z);
    float height = clampFloat(100.0f - std::sqrt(centerZ * centerZ + centerX * centerX) * 8.0f, -100.0f, 80.0f);

    for (int32_t i = -12; i <= 12; i++) {
        for (int32_t j = -12; j <= 12; j++) {
            const int64_t islandX = (int64_t) chunkX + i;
            const int64_t islandZ = (int64_t) chunkZ + j;
            if (islandX * islandX + islandZ * islandZ <= 4096LL
                || islandNoise.getValue((float) islandX, (float) islandZ) >= -0.9f)
                continue;

            const uint32_t mix = 3439u * (uint32_t) std::abs((int32_t) islandX)
                                 + 147u * (uint32_t) std::abs((int32_t) islandZ);
            const float dx = (float) (x - i * 2);
            const float dz = (float) (z - j * 2);
            const float island = 100.0f - std::sqrt(dz * dz + dx * dx) * (float) (mix % 13u + 9u);
            height = std::max(clampFloat(island, -100.0f, 80.0f), height);
        }
    }

    return height;
}

template <typename Visitor>
void TheEndGenerator::_forEachTerrainBlock(int32_t chunkX, int32_t chunkZ, Visitor &&visit) const {
    const TheEndTerrainNoises &noises = mNoiseHolder->getTerrainNoises();
    const int32_t xzCount = TheEndGeneratorConstants::DENSITY_XZ_COUNT;
    const int32_t yCount = TheEndGeneratorConstants::DENSITY_Y_COUNT;
    const int32_t total = xzCount * xzCount * yCount;

    const float position[3] = {(float) (chunkX * 2), 0.0f, (float) (chunkZ * 2)};
    std::vector<float> detail((size_t) total, 0.0f);
    std::vector<float> roughness((size_t) total, 0.0f);
    std::vector<float> roughness2((size_t) total, 0.0f);
    noises.getDetailNoiseOctaves().generate(detail.data(), position, xzCount, yCount, xzCount, 17.11f, 4.2775f);
    noises.getRoughnessNoiseOctaves().generate(roughness.data(), position, xzCount, yCount, xzCount, 1368.824f,
                                               684.412f);
    noises.getRoughnessNoiseOctaves2().generate(roughness2.data(), position, xzCount, yCount, xzCount, 1368.824f,
                                                684.412f);

    std::vector<float> density((size_t) total, 0.0f);
    for (int32_t i = 0; i < xzCount; i++) {
        for (int32_t j = 0; j < xzCount; j++) {
            const float island = getIslandHeight(chunkX, chunkZ, i, j, noises.getIslandNoise());
            for (int32_t k = 0; k < yCount; k++) {
                const size_t index = (size_t) ((i * xzCount + j) * yCount + k);
                const float low = roughness[index] * 0.001953125f;
                const float high = roughness2[index] * 0.001953125f;
                const float blend = (detail[index] / 10.0f + 1.0f) * 0.5f;
                float value = low;
                if (blend > 1.0f)
                    value = high;
                else if (blend >= 0.0f)
                    value = (high - low) * blend + low;

                value = (value + -8.0f) + island;
                if (k > 14) {
                    const float lowering = clampFloat((float) (k - 14) * 0.015625f, 0.0f, 1.0f);
                    value = lowering * -3000.0f + (1.0f - lowering) * value;
                } else if (k < 8) {
                    const float lowering = (float) (8 - k) / 7.0f;
                    value = lowering * -30.0f + (1.0f - lowering) * value;
                }

                density[index] = value;
            }
        }
    }

    for (int32_t i = 0; i < 2; i++) {
        for (int32_t j = 0; j < 2; j++) {
            for (int32_t k = 0; k < yCount - 1; k++) {
                const auto at = [&](int32_t x, int32_t z, int32_t y) {
                    return density[(size_t) (((i + x) * xzCount + j + z) * yCount + k + y)];
                };

                for (int32_t localZ = 0; localZ < 8; localZ++) {
                    const float stepZ = (float) localZ * 0.125f;
                    const float low0 = stepZ * (at(0, 1, 0) - at(0, 0, 0)) + at(0, 0, 0);
                    const float high0 = stepZ * (at(0, 1, 1) - at(0, 0, 1)) + at(0, 0, 1);
                    const float low1 = stepZ * (at(1, 1, 0) - at(1, 0, 0)) + at(1, 0, 0);
                    const float high1 = stepZ * (at(1, 1, 1) - at(1, 0, 1)) + at(1, 0, 1);

                    for (int32_t localX = 0; localX < 8; localX++) {
                        const float stepX = (float) localX * 0.125f;
                        const float low = stepX * (low1 - low0) + low0;
                        const float high = stepX * (high1 - high0) + high0;
                        const float slope = high - low;

                        for (int32_t localY = 0; localY < 4; localY++) {
                            if ((float) localY * 0.25f * slope + low <= 0.0f)
                                continue;

                            visit((i << 3) + localX, (k << 2) + localY, (j << 3) + localZ);
                        }
                    }
                }
            }
        }
    }
}

void TheEndGenerator::_generateTerrain(LevelChunk &chunk, int32_t *heightMap) const {
    _forEachTerrainBlock(chunk.getX(), chunk.getZ(), [&chunk, heightMap](int32_t x, int32_t y, int32_t z) {
        chunk.setBlock(x, y, z, endStoneState());
        heightMap[(x << 4) | z] = std::max(heightMap[(x << 4) | z], y);
    });
}

int32_t TheEndGenerator::getTerrainTop(int32_t chunkX, int32_t chunkZ) const {
    int32_t top = 0;
    _forEachTerrainBlock(chunkX, chunkZ, [&top](int32_t x, int32_t y, int32_t z) {
        (void) x;
        (void) z;
        top = std::max(top, y);
    });
    return top;
}

void TheEndGenerator::generate(LevelChunk &chunk) const {
    int32_t heightMap[256];
    for (int32_t i = 0; i < 256; i++)
        heightMap[i] = 0;

    _generateTerrain(chunk, heightMap);

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++)
            chunk.setHeight(x, z, heightMap[(x << 4) | z]);
    }

    chunk.setBiome((uint32_t) TheEndGeneratorConstants::BIOME_ID);

    chunk.clearDirty();
}

void TheEndGenerator::decorate(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) const {
    TheEndFeatureBuilder::apply(level, chunk, *mNoiseHolder, overflow);
}
