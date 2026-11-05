#include "World/Level/LevelGen/Overworld/OverworldGenerator.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Biome/BiomeZoom.h"
#include "World/Level/LevelGen/Overworld/Biome/OverworldBiomeParameters.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldFeatureBuilder.h"
#include "World/Level/LevelGen/Feature/FeatureNoiseHolder.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"
#include "World/Level/LevelGen/Overworld/Surface/OverworldSurfaceBuilder.h"
#include "World/Level/LevelGen/Overworld/Terrain/OverworldClimate.h"
#include "World/Level/LevelGen/Overworld/Terrain/OverworldTerrainFiller.h"
#include "World/Level/LevelGen/Overworld/Surface/SurfaceOverwriteBuilder.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Protocol/BlockStateHasher.h"

#include <cmath>
#include <cstdlib>
#include <vector>

namespace {
    const BlockState &bedrockState() {
        static const BlockState state = VanillaBlocks::BEDROCK().toBlockState();
        return state;
    }

    bool isAirState(const BlockState &state) {
        return state.mName == "minecraft:air";
    }

    // biome samples every 8 blocks from 8 blocks before the chunk, 2 extra in y (sub_147186040)
    constexpr int32_t BIOME_SAMPLE_COUNT_XZ = 4;
    constexpr int32_t BIOME_SAMPLE_COUNT_Y =
            ((OverworldGeneratorConstants::MAX_Y + 1 - OverworldGeneratorConstants::MIN_Y) >> 3) + 2;

    float biomeDepth(int32_t y, float offset) {
        return static_cast<float>(static_cast<int16_t>(y)) * -0.0078125f + 1.0f + offset;
    }
}

OverworldGenerator::OverworldGenerator(int64_t seed)
        : mSeed(seed), mAirHash(BlockStateHasher::hash("minecraft:air")) {
    mNoiseHolder = std::make_unique<OverworldNoiseHolder>(seed);

    const IRandomPtr surfaceRandom = mNoiseHolder->getRandomSource().identical();
    mSurfaceBuilder = std::make_unique<OverworldSurfaceBuilder>(mNoiseHolder->getNoiseFactory());
    mSurfaceOverwriteBuilder = std::make_unique<SurfaceOverwriteBuilder>(*surfaceRandom, mNoiseHolder->getNoiseFactory(),
                                                                         seed);

    FeatureNoiseHolder::initialize(mNoiseHolder->getRandomSource());

    mClimate = std::make_unique<OverworldClimate>(mNoiseHolder->getNoiseFactory());
    mTerrainFiller = std::make_unique<OverworldTerrainFiller>(mNoiseHolder->getNoiseFactory(), *mClimate);
    mBiomeParameters = std::make_unique<OverworldBiomeParameters>();
}

OverworldGenerator::~OverworldGenerator() = default;

// climate only, without the zoom: the game's biome search samples the same way (sub_147186890 -> sub_14CDA32A0)
int32_t OverworldGenerator::pickBiome(int32_t x, int32_t y, int32_t z) const {
    const OverworldClimate::Column column = mClimate->sampleColumn(x >> 2, z >> 2);
    const OverworldBiomeParameters::Target target = OverworldBiomeParameters::target(
            column.mTemperature, column.mVegetation, column.mContinentalness, column.mErosion,
            biomeDepth(y, column.mOffset), column.mRidges);
    return mBiomeParameters->search(target, nullptr)->mBiome;
}

void OverworldGenerator::decorate(Level &level, LevelChunk &chunk,
                                 std::vector<GeneratedBlockChange> *overflow) const {
    OverworldFeatureBuilder::apply(level, chunk, overflow);
}

int64_t OverworldGenerator::chunkHash(int32_t chunkX, int32_t chunkZ) {
    return (int64_t) (((uint64_t) (uint32_t) chunkX << 32) | (uint64_t) (uint32_t) chunkZ);
}

int64_t OverworldGenerator::parseSeed(const std::string &value) {
    if (value.empty())
        return 0;

    char *end = nullptr;
    const long long parsed = std::strtoll(value.c_str(), &end, 10);
    if (end != nullptr && *end == '\0')
        return (int64_t) parsed;

    // Non-numeric seeds are reduced to a 32-bit seed with the 31-multiplier string hash.
    int32_t hash = 0;
    for (char character: value)
        hash = (int32_t) ((uint32_t) hash * 31u + (uint32_t) (unsigned char) character);

    return (int64_t) hash;
}

void OverworldGenerator::_generateTerrain(LevelChunk &chunk, int32_t *heightMap) const {
    const int32_t minY = OverworldGeneratorConstants::MIN_Y;
    mTerrainFiller->fill(chunk, heightMap);

    SimpleRandom random(mSeed ^ chunkHash(chunk.getX(), chunk.getZ()));
    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            chunk.setBlock(x, minY, z, bedrockState());

            const int32_t bedrockDepth = random.nextBoundedInt(6);
            for (int32_t i = 0; i < bedrockDepth; i++) {
                const int32_t y = minY + i;
                if (!isAirState(chunk.getBlock(x, y, z)))
                    chunk.setBlock(x, y, z, bedrockState());
            }
        }
    }
}

void OverworldGenerator::_generateBiomeMap(LevelChunk &chunk) const {
    const int32_t originX = (chunk.getX() << 4) - 8;
    const int32_t originY = OverworldGeneratorConstants::MIN_Y - 8;
    const int32_t originZ = (chunk.getZ() << 4) - 8;

    // sub_147185D00, sub_1471AC680
    std::vector<int32_t> samples((size_t) (BIOME_SAMPLE_COUNT_XZ * BIOME_SAMPLE_COUNT_XZ * BIOME_SAMPLE_COUNT_Y));
    size_t index = 0;
    for (int32_t indexX = 0; indexX < BIOME_SAMPLE_COUNT_XZ; indexX++) {
        const int32_t x = originX + 8 * indexX;
        for (int32_t indexZ = 0; indexZ < BIOME_SAMPLE_COUNT_XZ; indexZ++) {
            const int32_t z = originZ + 8 * indexZ;
            const OverworldClimate::Column column = mClimate->sampleColumn(x >> 2, z >> 2);
            const OverworldBiomeParameters::Node *last = nullptr;
            for (int32_t indexY = 0; indexY < BIOME_SAMPLE_COUNT_Y; indexY++) {
                const int32_t y = originY + 8 * indexY;
                const OverworldBiomeParameters::Target target = OverworldBiomeParameters::target(
                        column.mTemperature, column.mVegetation, column.mContinentalness, column.mErosion,
                        biomeDepth(y, column.mOffset), column.mRidges);
                last = mBiomeParameters->search(target, last);
                samples[index++] = last->mBiome;
            }
        }
    }

    // the zoom takes the level's 32-bit seed (vfunc +0x430 in sub_147186040)
    const BiomeZoom zoom((uint32_t) mSeed, originX, originY, originZ, BIOME_SAMPLE_COUNT_XZ, BIOME_SAMPLE_COUNT_Y,
                         BIOME_SAMPLE_COUNT_XZ);
    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            for (int32_t y = OverworldGeneratorConstants::MIN_Y; y <= OverworldGeneratorConstants::MAX_Y; y++) {
                const int32_t sample = zoom.nearestSample((chunk.getX() << 4) + x, y, (chunk.getZ() << 4) + z);
                chunk.setBiomeAt(x, y, z, (uint32_t) samples[(size_t) sample]);
            }
        }
    }
}

void OverworldGenerator::generate(LevelChunk &chunk) const {
    int32_t heightMap[256];
    for (int32_t i = 0; i < 256; i++)
        heightMap[i] = 0;

    _generateTerrain(chunk, heightMap);

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++)
            chunk.setHeight(x, z, heightMap[(x << 4) | z]);
    }

    _generateBiomeMap(chunk);

    mSurfaceBuilder->apply(chunk);

    mSurfaceOverwriteBuilder->apply(chunk, mSeed);

    chunk.clearDirty();
}
