#include "World/Level/LevelGen/Overworld/OverworldGenerator.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Overworld/Biome/OverworldBiomeResult.h"
#include "World/Level/LevelGen/Overworld/Feature/OverworldFeatureBuilder.h"
#include "World/Level/LevelGen/Overworld/Biome/VanillaOverworldBiomeSource.h"
#include "World/Level/LevelGen/Density/DensityCommon.h"
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

namespace {
    const BlockState &bedrockState() {
        static const BlockState state = VanillaBlocks::BEDROCK().toBlockState();
        return state;
    }

    bool isAirState(const BlockState &state) {
        return state.mName == "minecraft:air";
    }
}

OverworldGenerator::OverworldGenerator(int64_t seed)
        : mSeed(seed), mAirHash(BlockStateHasher::hash("minecraft:air")) {
    mNoiseHolder = std::make_unique<OverworldNoiseHolder>(seed);

    const OverworldTerrainNoises &terrain = mNoiseHolder->getTerrainNoises();
    const OverworldBiomeNoises &biome = mNoiseHolder->getBiomeNoises();

    mBiomeSource = std::make_unique<VanillaOverworldBiomeSource>(
            terrain.getContinents(),
            terrain.getErosion(),
            terrain.getRidges(),
            biome.getTemperatureNoise(),
            biome.getHumidityNoise()
    );

    const IRandomPtr surfaceRandom = mNoiseHolder->getRandomSource().identical();
    mSurfaceBuilder = std::make_unique<OverworldSurfaceBuilder>(mNoiseHolder->getNoiseFactory());
    mSurfaceOverwriteBuilder = std::make_unique<SurfaceOverwriteBuilder>(*surfaceRandom, mNoiseHolder->getNoiseFactory(),
                                                                         seed);

    FeatureNoiseHolder::initialize(mNoiseHolder->getRandomSource());

    mClimate = std::make_unique<OverworldClimate>(mNoiseHolder->getNoiseFactory());
    mTerrainFiller = std::make_unique<OverworldTerrainFiller>(mNoiseHolder->getNoiseFactory(), *mClimate);
}

OverworldGenerator::~OverworldGenerator() = default;

int32_t OverworldGenerator::pickBiome(int32_t x, int32_t y, int32_t z) const {
    return mBiomeSource->pickBiomeId(x, y, z);
}

OverworldBiomeResult OverworldGenerator::pickBiomeResult(int32_t x, int32_t y, int32_t z) const {
    return mBiomeSource->pickRaw(x, y, z);
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

void OverworldGenerator::_generateBiomeMap(LevelChunk &chunk, DensityChunkCache &chunkCache,
                                           const int32_t *heightMap) const {
    const int32_t chunkX = chunk.getX();
    const int32_t chunkZ = chunk.getZ();

    chunkCache.clear();
    CellFunctionContext functionContext(&chunkCache);

    std::vector<OverworldBiomeResult> biomes;
    biomes.reserve(256);

    for (int32_t localX = 0; localX < 16; localX++) {
        const int32_t x = chunkX * 16 + localX;
        for (int32_t localZ = 0; localZ < 16; localZ++) {
            const int32_t z = chunkZ * 16 + localZ;
            biomes.push_back(mBiomeSource->pick(
                    x,
                    OverworldGeneratorConstants::SEA_LEVEL,
                    z,
                    functionContext.set(x, OverworldGeneratorConstants::SEA_LEVEL, z),
                    heightMap[(localX << 4) | localZ]
            ));
        }
    }

    for (int32_t y = OverworldGeneratorConstants::MAX_Y; y >= OverworldGeneratorConstants::MIN_Y; y--) {
        for (int32_t x = 0; x < 16; x++) {
            for (int32_t z = 0; z < 16; z++) {
                OverworldBiomeResult &result = biomes[(size_t) (x * 16 + z)];
                result.correct(y - heightMap[(x << 4) | z]);
                chunk.setBiomeAt(x, y, z, (uint32_t) result.getBiomeId());
                result.reset();
            }
        }
    }
}

void OverworldGenerator::generate(LevelChunk &chunk) const {
    DensityChunkCache chunkCache;
    chunkCache.clear();

    int32_t heightMap[256];
    for (int32_t i = 0; i < 256; i++)
        heightMap[i] = 0;

    _generateTerrain(chunk, heightMap);

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++)
            chunk.setHeight(x, z, heightMap[(x << 4) | z]);
    }

    _generateBiomeMap(chunk, chunkCache, heightMap);

    mSurfaceBuilder->apply(chunk);

    mSurfaceOverwriteBuilder->apply(chunk, mSeed);

    chunk.clearDirty();
}
