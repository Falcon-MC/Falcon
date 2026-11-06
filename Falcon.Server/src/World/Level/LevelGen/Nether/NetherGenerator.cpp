#include "World/Level/LevelGen/Nether/NetherGenerator.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Nether/Biome/VanillaNetherBiomeSource.h"
#include "World/Level/LevelGen/Nether/Feature/NetherFeatureBuilder.h"
#include "World/Level/LevelGen/Nether/NetherCarvers.h"
#include "World/Level/LevelGen/Nether/Surface/NetherSurfaceBuilder.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Protocol/BlockStateHasher.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {
    // bit patterns read from the game, one unit off the nearest float of the decimal values
    const float LIMIT_XZ_SCALE = 0x1.5634bcp+9f;
    const float LIMIT_Y_SCALE = 0x1.00a78cp+11f;
    const float MAIN_XZ_SCALE = 0x1.11c3cap+3f;
    const float MAIN_Y_SCALE = 0x1.11c3c8p+5f;

    const BlockState &netherrackState() {
        static const BlockState state = VanillaBlocks::NETHERRACK().toBlockState();
        return state;
    }

    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }

    bool isAirState(const BlockState &state) {
        return state.mName == "minecraft:air";
    }
}

NetherGenerator::NetherGenerator(int64_t seed)
        : mSeed(seed), mAirHash(BlockStateHasher::hash("minecraft:air")) {
    mNoiseHolder = std::make_unique<NetherNoiseHolder>(seed);
    mBiomeSource = std::make_unique<VanillaNetherBiomeSource>(seed);

    const NetherTerrainNoises &noises = mNoiseHolder->getTerrainNoises();
    mSurfaceBuilder = std::make_unique<NetherSurfaceBuilder>((uint32_t) seed, noises.getDepthNoise(),
                                                             noises.getAdjustmentNoise());

    mCarvers = std::make_unique<NetherCarvers>(seed);

    NetherNoiseHolder::setInstance(mNoiseHolder.get());
}

NetherGenerator::~NetherGenerator() {
    if (NetherNoiseHolder::get() == mNoiseHolder.get())
        NetherNoiseHolder::setInstance(nullptr);
}

int64_t NetherGenerator::chunkHash(int32_t chunkX, int32_t chunkZ) {
    return (int64_t) (((uint64_t) (uint32_t) chunkX << 32) | (uint64_t) (uint32_t) chunkZ);
}

int32_t NetherGenerator::pickBiome(int32_t x, int32_t y, int32_t z) const {
    return mBiomeSource->pickBiomeId(x, y, z);
}

void NetherGenerator::decorate(Level &level, LevelChunk &chunk,
                               std::vector<GeneratedBlockChange> *overflow) const {
    NetherFeatureBuilder::apply(level, chunk, overflow);
}

void NetherGenerator::_generateBiomeMap(LevelChunk &chunk) const {
    int32_t biomes[256];
    mBiomeSource->pickChunk(chunk.getX(), chunk.getZ(), biomes);

    for (int32_t y = NetherGeneratorConstants::MAX_Y; y >= NetherGeneratorConstants::MIN_Y; y--) {
        for (int32_t x = 0; x < 16; x++) {
            for (int32_t z = 0; z < 16; z++)
                chunk.setBiomeAt(x, y, z, (uint32_t) biomes[(x << 4) | z]);
        }
    }
}

void NetherGenerator::_generateTerrain(LevelChunk &chunk) const {
    const NetherTerrainNoises &noises = mNoiseHolder->getTerrainNoises();
    const int32_t xzCount = NetherGeneratorConstants::DENSITY_XZ_COUNT;
    const int32_t yCount = NetherGeneratorConstants::DENSITY_Y_COUNT;
    const size_t total = (size_t) (xzCount * xzCount * yCount);

    const float position[3] = {(float) (chunk.getX() * 4), 0.0f, (float) (chunk.getZ() * 4)};
    std::vector<float> minLimit(total, 0.0f);
    std::vector<float> maxLimit(total, 0.0f);
    std::vector<float> main(total, 0.0f);
    noises.getMainNoise().generate(main.data(), position, xzCount, yCount, xzCount, MAIN_XZ_SCALE, MAIN_Y_SCALE);
    noises.getMinLimitNoise().generate(minLimit.data(), position, xzCount, yCount, xzCount, LIMIT_XZ_SCALE,
                                       LIMIT_Y_SCALE);
    noises.getMaxLimitNoise().generate(maxLimit.data(), position, xzCount, yCount, xzCount, LIMIT_XZ_SCALE,
                                       LIMIT_Y_SCALE);

    float offsets[NetherGeneratorConstants::DENSITY_Y_COUNT];
    for (int32_t k = 0; k < yCount; k++) {
        const float distance = k >= 9 ? 16.0f - (float) k : (float) k;
        const float index = (float) k * 3.1415927f * 6.0f / 17.0f * 10430.378f + 16384.0f;
        const float cosine = (float) std::sin((double) (uint16_t) (int32_t) index * 6.283185307179586 / 65536.0);
        offsets[k] = cosine + cosine;
        if (distance < 4.0f)
            offsets[k] += (4.0f - distance) * (4.0f - distance) * (4.0f - distance) * -10.0f;
    }

    std::vector<float> density(total, 0.0f);
    for (size_t index = 0; index < total; index++) {
        const int32_t k = (int32_t) (index % (size_t) yCount);
        const float low = minLimit[index] * 0.001953125f;
        const float high = maxLimit[index] * 0.001953125f;
        const float blend = (main[index] / 10.0f + 1.0f) * 0.5f;
        float value = low;
        if (blend > 1.0f)
            value = high;
        else if (blend >= 0.0f)
            value = (high - low) * blend + low;

        value -= offsets[k];
        if (k == 14)
            value = value * 0.6666666f + -3.3333335f;
        else if (k == 15)
            value = value * 0.3333333f + -6.666667f;
        else if (k == 16)
            value = value * 0.0f + -10.0f;

        density[index] = value;
    }

    for (int32_t cellX = 0; cellX < 4; cellX++) {
        for (int32_t cellZ = 0; cellZ < 4; cellZ++) {
            for (int32_t k = 0; k < yCount - 1; k++) {
                const auto at = [&](int32_t x, int32_t z, int32_t y) {
                    return density[(size_t) (((cellX + x) * xzCount + cellZ + z) * yCount + k + y)];
                };

                for (int32_t localZ = 0; localZ < 4; localZ++) {
                    const float stepZ = (float) localZ * 0.25f;
                    const float low0 = (at(0, 1, 0) - at(0, 0, 0)) * stepZ + at(0, 0, 0);
                    const float high0 = (at(0, 1, 1) - at(0, 0, 1)) * stepZ + at(0, 0, 1);
                    const float lowDelta = ((at(1, 1, 0) - at(1, 0, 0)) * stepZ + at(1, 0, 0)) - low0;
                    const float highDelta = ((at(1, 1, 1) - at(1, 0, 1)) * stepZ + at(1, 0, 1)) - high0;

                    for (int32_t localX = 0; localX < 4; localX++) {
                        const float stepX = (float) localX * 0.25f;
                        const float low = lowDelta * stepX + low0;
                        const float slope = (stepX * highDelta + high0) - low;

                        for (int32_t localY = 0; localY < NetherGeneratorConstants::CELL_HEIGHT; localY++) {
                            const int32_t x = cellX * 4 + localX;
                            const int32_t y = k * NetherGeneratorConstants::CELL_HEIGHT + localY;
                            const int32_t z = cellZ * 4 + localZ;
                            if (slope * ((float) localY * 0.125f) + low > 0.0f)
                                chunk.setBlock(x, y, z, netherrackState());
                            else if (y <= NetherGeneratorConstants::LAVA_LEVEL)
                                chunk.setBlock(x, y, z, lavaState());
                        }
                    }
                }
            }
        }
    }
}

void NetherGenerator::generate(LevelChunk &chunk) const {
    _generateBiomeMap(chunk);

    _generateTerrain(chunk);

    mSurfaceBuilder->apply(chunk);

    mCarvers->carve(chunk);

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            int32_t height = NetherGeneratorConstants::MIN_Y;
            for (int32_t y = NetherGeneratorConstants::MAX_Y; y >= NetherGeneratorConstants::MIN_Y; y--) {
                if (!isAirState(chunk.getBlock(x, y, z))) {
                    height = y;
                    break;
                }
            }

            chunk.setHeight(x, z, height);
        }
    }

    chunk.clearDirty();
}
