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

    double clampDouble(double value, double min, double max) {
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
    float x1 = (float) (chunkX * 2 + x);
    float z1 = (float) (chunkZ * 2 + z);
    float islandHeight1 = clampFloat(100.0f - std::sqrt((x1 * x1) + (z1 * z1)) * 8.0f, -100.0f, 80.0f);

    for (int32_t i = -12; i <= 12; i++) {
        for (int32_t j = -12; j <= 12; j++) {
            const int64_t x2 = (int64_t) chunkX + (int64_t) i;
            const int64_t z2 = (int64_t) chunkZ + (int64_t) j;
            if ((x2 * x2) + (z2 * z2) > 4096LL && islandNoise.getValue((double) x2, (double) z2) < (double) -0.9f) {
                x1 = (float) (x - i * 2);
                z1 = (float) (z - j * 2);
                float islandHeight2 = 100.0f - std::sqrt((x1 * x1) + (z1 * z1))
                                               * (std::fmod(std::fabs((float) x2) * 3439.0f
                                                            + std::fabs((float) z2) * 147.0f, 13.0f) + 9.0f);
                islandHeight2 = clampFloat(islandHeight2, -100.0f, 80.0f);
                islandHeight1 = std::max(islandHeight1, islandHeight2);
            }
        }
    }

    return islandHeight1;
}

void TheEndGenerator::_generateTerrain(LevelChunk &chunk, int32_t *heightMap) const {
    const TheEndTerrainNoises &noises = mNoiseHolder->getTerrainNoises();

    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();
    const int32_t densityX = chunkX << 1;
    const int32_t densityZ = chunkZ << 1;

    const int32_t xzCount = TheEndGeneratorConstants::DENSITY_XZ_COUNT;
    const int32_t yCount = TheEndGeneratorConstants::DENSITY_Y_COUNT;
    const double coordinateScale = TheEndGeneratorConstants::COORDINATE_SCALE;

    const std::vector<double> detailNoise = noises.getDetailNoiseOctaves().generateNoiseOctaves(
            densityX, 0, densityZ, xzCount, yCount, xzCount,
            (coordinateScale * 2.0) / TheEndGeneratorConstants::DETAIL_NOISE_SCALE_X,
            TheEndGeneratorConstants::DETAIL_NOISE_Y_SCALE,
            (coordinateScale * 2.0) / TheEndGeneratorConstants::DETAIL_NOISE_SCALE_Z);
    const std::vector<double> roughnessNoise = noises.getRoughnessNoiseOctaves().generateNoiseOctaves(
            densityX, 0, densityZ, xzCount, yCount, xzCount,
            coordinateScale * 2.0, coordinateScale, coordinateScale * 2.0);
    const std::vector<double> roughnessNoise2 = noises.getRoughnessNoiseOctaves2().generateNoiseOctaves(
            densityX, 0, densityZ, xzCount, yCount, xzCount,
            coordinateScale * 2.0, coordinateScale, coordinateScale * 2.0);

    std::vector<double> density((size_t) (xzCount * xzCount * yCount), 0.0);

    int32_t index = 0;

    for (int32_t i = 0; i < xzCount; i++) {
        for (int32_t j = 0; j < xzCount; j++) {
            const float noiseHeight = getIslandHeight(chunkX, chunkZ, i, j, noises.getIslandNoise());
            for (int32_t k = 0; k < yCount; k++) {
                const double noiseR = roughnessNoise[(size_t) index] / 512.0;
                const double noiseR2 = roughnessNoise2[(size_t) index] / 512.0;
                const double noiseD = (detailNoise[(size_t) index] / 10.0 + 1.0) / 2.0;
                double dens = 0.0;
                if (noiseD < 0.0) {
                    dens = noiseR;
                } else if (noiseD > 1.0) {
                    dens = noiseR2;
                } else {
                    dens = noiseR + (noiseR2 - noiseR) * noiseD;
                }

                dens = (dens - 8.0) + (double) noiseHeight;
                index++;

                double lowering = 0.0;
                if (k < 8) {
                    lowering = (double) ((float) (8 - k) / 7.0f);
                    dens = dens * (1.0 - lowering) + lowering * -30.0;
                } else if (k > yCount / 2 - 2) {
                    lowering = (double) ((float) (k - ((yCount / 2) - 2)) / 64.0);
                    lowering = clampDouble(lowering, 0.0, 1.0);
                    dens = dens * (1.0 - lowering) + lowering * -3000.0;
                }

                density[(size_t) (((i * xzCount) + j) * yCount + k)] = dens;
            }
        }
    }

    for (int32_t i = 0; i < xzCount - 1; i++) {
        for (int32_t j = 0; j < xzCount - 1; j++) {
            for (int32_t k = 0; k < yCount - 1; k++) {
                double d1 = density[(size_t) (((i * xzCount) + j) * yCount + k)];
                double d2 = density[(size_t) ((((i + 1) * xzCount) + j) * yCount + k)];
                double d3 = density[(size_t) (((i * xzCount) + j + 1) * yCount + k)];
                double d4 = density[(size_t) ((((i + 1) * xzCount) + j + 1) * yCount + k)];
                const double d5 = (density[(size_t) (((i * xzCount) + j) * yCount + k + 1)] - d1) / 4.0;
                const double d6 = (density[(size_t) ((((i + 1) * xzCount) + j) * yCount + k + 1)] - d2) / 4.0;
                const double d7 = (density[(size_t) (((i * xzCount) + j + 1) * yCount + k + 1)] - d3) / 4.0;
                const double d8 = (density[(size_t) ((((i + 1) * xzCount) + j + 1) * yCount + k + 1)] - d4) / 4.0;

                for (int32_t l = 0; l < 4; l++) {
                    double d9 = d1;
                    double d10 = d3;
                    for (int32_t m = 0; m < 8; m++) {
                        double dens = d9;
                        for (int32_t n = 0; n < 8; n++) {
                            if (dens > 0.0) {
                                const int32_t localX = m + (i << 3);
                                const int32_t y = l + (k << 2);
                                const int32_t localZ = n + (j << 3);
                                chunk.setBlock(localX, y, localZ, endStoneState());
                                if (y > heightMap[(localX << 4) | localZ])
                                    heightMap[(localX << 4) | localZ] = y;
                            }

                            dens += (d10 - d9) / 8.0;
                        }

                        d9 += (d2 - d1) / 8.0;
                        d10 += (d4 - d3) / 8.0;
                    }

                    d1 += d5;
                    d3 += d7;
                    d2 += d6;
                    d4 += d8;
                }
            }
        }
    }
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
