#include "Level/Generator/Overworld/OverworldGenerator.h"

#include "Block/Blocks/VanillaBlocks.h"
#include "Level/Generator/Overworld/Biome/OverworldBiomeResult.h"
#include "Level/Generator/Overworld/Feature/OverworldFeatureBuilder.h"
#include "Level/Generator/Overworld/Biome/VanillaOverworldBiomeSource.h"
#include "Level/Generator/Density/DensityCommon.h"
#include "Level/Generator/Feature/FeatureNoiseHolder.h"
#include "Level/Generator/Material/Aquifer.h"
#include "Level/Generator/Material/MultiMaterial.h"
#include "Level/Generator/Material/OreVeinifier.h"
#include "Level/Generator/Random/SimpleRandom.h"
#include "Level/Generator/Overworld/Surface/OverworldSurfaceBuilder.h"
#include "Level/Generator/Overworld/Surface/SurfaceOverwriteBuilder.h"
#include "Level/LevelChunk.h"
#include "Protocol/BlockStateHasher.h"

#include <cmath>
#include <cstdlib>
#include <deque>
#include <vector>

namespace {
    const BlockState &stoneState() {
        static const BlockState state = VanillaBlocks::STONE().toBlockState();
        return state;
    }

    const BlockState &deepslateState() {
        static const BlockState state = VanillaBlocks::DEEPSLATE().toBlockState();
        return state;
    }

    const BlockState &bedrockState() {
        static const BlockState state = VanillaBlocks::BEDROCK().toBlockState();
        return state;
    }

    bool isAirState(const BlockState &state) {
        return state.mName == "minecraft:air";
    }

    int32_t floorDiv(int32_t value, int32_t divisor) {
        int32_t quotient = value / divisor;
        if ((value % divisor != 0) && ((value < 0) != (divisor < 0)))
            quotient--;

        return quotient;
    }

    int32_t cellIndexOf(int32_t cellXIndex, int32_t cellYIndex, int32_t cellZIndex) {
        return (cellYIndex * OverworldGeneratorConstants::CELL_Z_COUNT + cellZIndex)
               * OverworldGeneratorConstants::CELL_X_COUNT + cellXIndex;
    }

    /**
     * Stone turns into deepslate below y 0, with a dithered transition between y 0 and 8
     * where the chance of deepslate drops as y rises.
     */
    bool shouldPlaceDeepslate(IRandom &random, int32_t y) {
        if (y < 0)
            return true;

        if (y > 8)
            return false;

        return random.nextBoundedInt(9) >= y;
    }

    bool isNorthWestCornerSeed(int32_t cellXIndex, int32_t cellZIndex) {
        return (cellXIndex == 0 && cellZIndex == 0)
               || (cellXIndex == 1 && cellZIndex == 0)
               || (cellXIndex == 0 && cellZIndex == 1);
    }

    bool isNorthEastCornerSeed(int32_t cellXIndex, int32_t cellZIndex) {
        return (cellXIndex == OverworldGeneratorConstants::CELL_X_COUNT - 1 && cellZIndex == 0)
               || (cellXIndex == OverworldGeneratorConstants::CELL_X_COUNT - 2 && cellZIndex == 0)
               || (cellXIndex == OverworldGeneratorConstants::CELL_X_COUNT - 1 && cellZIndex == 1);
    }

    bool isSouthWestCornerSeed(int32_t cellXIndex, int32_t cellZIndex) {
        return (cellXIndex == 0 && cellZIndex == OverworldGeneratorConstants::CELL_Z_COUNT - 1)
               || (cellXIndex == 1 && cellZIndex == OverworldGeneratorConstants::CELL_Z_COUNT - 1)
               || (cellXIndex == 0 && cellZIndex == OverworldGeneratorConstants::CELL_Z_COUNT - 2);
    }

    bool isSouthEastCornerSeed(int32_t cellXIndex, int32_t cellZIndex) {
        return (cellXIndex == OverworldGeneratorConstants::CELL_X_COUNT - 1
                && cellZIndex == OverworldGeneratorConstants::CELL_Z_COUNT - 1)
               || (cellXIndex == OverworldGeneratorConstants::CELL_X_COUNT - 2
                   && cellZIndex == OverworldGeneratorConstants::CELL_Z_COUNT - 1)
               || (cellXIndex == OverworldGeneratorConstants::CELL_X_COUNT - 1
                   && cellZIndex == OverworldGeneratorConstants::CELL_Z_COUNT - 2);
    }

    bool isCornerFloodSeedCell(int32_t cellXIndex, int32_t cellYIndex, int32_t cellY, int32_t cellZIndex) {
        if (cellY > OverworldGeneratorConstants::CORNER_FLOOD_SEED_MAX_Y)
            return false;

        if (cellYIndex % 3 != 0)
            return false;

        return isNorthWestCornerSeed(cellXIndex, cellZIndex)
               || isNorthEastCornerSeed(cellXIndex, cellZIndex)
               || isSouthWestCornerSeed(cellXIndex, cellZIndex)
               || isSouthEastCornerSeed(cellXIndex, cellZIndex);
    }

    bool shouldGenerateMandatoryCell(const int32_t *mandatoryTopY, int32_t cellX, int32_t cellY, int32_t cellZ) {
        if (cellY + OverworldGeneratorConstants::CELL_HEIGHT - 1 <= OverworldGeneratorConstants::SEA_LEVEL)
            return true;

        for (int32_t localX = 0; localX < OverworldGeneratorConstants::CELL_XZ_SIZE; localX++) {
            const int32_t x = cellX + localX;
            for (int32_t localZ = 0; localZ < OverworldGeneratorConstants::CELL_XZ_SIZE; localZ++) {
                const int32_t z = cellZ + localZ;
                if (cellY <= mandatoryTopY[(x << 4) | z])
                    return true;
            }
        }

        return false;
    }

    void enqueueCell(std::deque<int32_t> &queue, std::vector<bool> &queued, int32_t cellXIndex, int32_t cellYIndex,
                     int32_t cellZIndex, int32_t cellYCount) {
        if (cellXIndex < 0 || cellXIndex >= OverworldGeneratorConstants::CELL_X_COUNT
            || cellYIndex < 0 || cellYIndex >= cellYCount
            || cellZIndex < 0 || cellZIndex >= OverworldGeneratorConstants::CELL_Z_COUNT)
            return;

        const int32_t index = cellIndexOf(cellXIndex, cellYIndex, cellZIndex);
        if (queued[(size_t) index])
            return;

        queued[(size_t) index] = true;
        queue.push_back(index);
    }

    void enqueueNeighbors(std::deque<int32_t> &queue, std::vector<bool> &queued, int32_t cellXIndex,
                          int32_t cellYIndex, int32_t cellZIndex, int32_t cellYCount) {
        enqueueCell(queue, queued, cellXIndex + 1, cellYIndex, cellZIndex, cellYCount);
        enqueueCell(queue, queued, cellXIndex - 1, cellYIndex, cellZIndex, cellYCount);
        enqueueCell(queue, queued, cellXIndex, cellYIndex + 1, cellZIndex, cellYCount);
        enqueueCell(queue, queued, cellXIndex, cellYIndex - 1, cellZIndex, cellYCount);
        enqueueCell(queue, queued, cellXIndex, cellYIndex, cellZIndex + 1, cellYCount);
        enqueueCell(queue, queued, cellXIndex, cellYIndex, cellZIndex - 1, cellYCount);
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
    mSurfaceBuilder = std::make_unique<OverworldSurfaceBuilder>(*surfaceRandom);
    mSurfaceOverwriteBuilder = std::make_unique<SurfaceOverwriteBuilder>(*surfaceRandom, seed);

    FeatureNoiseHolder::initialize(mNoiseHolder->getRandomSource());
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

void OverworldGenerator::_generateTerrain(LevelChunk &chunk, DensityChunkCache &chunkCache,
                                          int32_t *heightMap) const {
    const OverworldTerrainNoises &terrain = mNoiseHolder->getTerrainNoises();

    const int32_t minY = OverworldGeneratorConstants::MIN_Y;
    const int32_t maxY = OverworldGeneratorConstants::MAX_Y;
    const int32_t yBlockSize = maxY + 1 - minY;
    const int32_t chunkBaseX = chunk.getX() << 4;
    const int32_t chunkBaseZ = chunk.getZ() << 4;
    const int32_t cellMinY = floorDiv(minY, 8) * 8;
    const int32_t cellMaxY = floorDiv(maxY, 8) * 8;
    const int32_t cellYCount = ((cellMaxY - cellMinY) / OverworldGeneratorConstants::CELL_HEIGHT) + 1;

    SimpleRandom random(mSeed ^ chunkHash(chunk.getX(), chunk.getZ()));

    CellFunctionContext functionContext(&chunkCache);

    int32_t mandatoryTopY[256];
    for (int32_t x = 0; x < 16; x++) {
        const int32_t worldX = chunkBaseX + x;
        for (int32_t z = 0; z < 16; z++) {
            const int32_t worldZ = chunkBaseZ + z;
            const int32_t upper = (int32_t) std::ceil(
                    terrain.getPreliminarySurfaceUpperBound()->compute(functionContext.set(worldX, 0, worldZ))
            );
            mandatoryTopY[(x << 4) | z] = std::min(maxY, std::max(OverworldGeneratorConstants::SEA_LEVEL, upper));
        }
    }

    Aquifer aquifer(
            chunk.getX(),
            chunk.getZ(),
            mSeed,
            &chunkCache,
            DensityCommon::noise(terrain.getBarrierNoise(), 1.0, 0.5),
            DensityCommon::noise(terrain.getFluidLevelFloodednessNoise(), 1.0, 0.67),
            DensityCommon::noise(terrain.getFluidLevelSpreadNoise(), 1.0, 0.7142857142857143),
            DensityCommon::noise(terrain.getLavaNoise(), 1.0, 1.0),
            terrain.getErosion(),
            terrain.getDepth(),
            terrain.getPreliminarySurfaceDensity(),
            terrain.getPreliminarySurfaceUpperBound(),
            -64,
            8,
            minY,
            yBlockSize,
            Aquifer::overworldFluidPicker(OverworldGeneratorConstants::SEA_LEVEL)
    );

    const DensityFunctionPtr wrappedDensity = terrain.getWrappedDensity();

    OreVeinifier oreVeinifier(
            terrain.getVeinToggle(),
            terrain.getVeinRidged(),
            terrain.getVeinGap(),
            terrain.getOreVeinifierSeed()
    );

    // Rules are tried in order and the first non-null block wins: the aquifer, then ore
    // veins, then plain stone wherever the density is positive.
    std::vector<MaterialRule> rules;
    rules.push_back([&aquifer, wrappedDensity](FunctionContext &context) -> const BlockState * {
        return aquifer.computeSubstance(context, wrappedDensity->compute(context));
    });
    rules.push_back([&oreVeinifier](FunctionContext &context) -> const BlockState * {
        return oreVeinifier.calculate(context);
    });
    rules.push_back([wrappedDensity](FunctionContext &context) -> const BlockState * {
        return wrappedDensity->compute(context) > 0.0 ? &stoneState() : nullptr;
    });

    const MultiMaterial multiMaterial(rules);

    const size_t cellCount = (size_t) (OverworldGeneratorConstants::CELL_X_COUNT * cellYCount
                                       * OverworldGeneratorConstants::CELL_Z_COUNT);
    std::vector<bool> queued(cellCount, false);
    std::vector<bool> solidMandatoryCells(cellCount, false);
    std::deque<int32_t> queue;

    const auto generateCell = [&](int32_t cellX, int32_t cellY, int32_t cellZ) -> bool {
        bool hasNonAir = false;

        for (int32_t localX = 0; localX < OverworldGeneratorConstants::CELL_XZ_SIZE; localX++) {
            const int32_t x = cellX + localX;
            const int32_t worldX = chunkBaseX + x;
            for (int32_t localZ = 0; localZ < OverworldGeneratorConstants::CELL_XZ_SIZE; localZ++) {
                const int32_t z = cellZ + localZ;
                const int32_t worldZ = chunkBaseZ + z;
                for (int32_t localY = OverworldGeneratorConstants::CELL_HEIGHT - 1; localY >= 0; localY--) {
                    const int32_t y = cellY + localY;
                    if (y < minY || y > maxY)
                        continue;

                    const BlockState *generatedState =
                            multiMaterial.calculate(functionContext.set(worldX, y, worldZ));

                    if (generatedState == nullptr)
                        continue;

                    const BlockState *placedState = generatedState;
                    if (placedState->getHash() == stoneState().getHash() && shouldPlaceDeepslate(random, y))
                        placedState = &deepslateState();

                    chunk.setBlock(x, y, z, *placedState);

                    if (!isAirState(*placedState)) {
                        hasNonAir = true;
                        if (y > heightMap[(x << 4) | z])
                            heightMap[(x << 4) | z] = y;
                    }
                }
            }
        }

        return hasNonAir;
    };

    // Evaluating the density for every block is the dominant cost of terrain generation, and
    // most of a column above the surface is air. Only cells up to sea level or the
    // preliminary surface estimate (plus a few seed cells) are evaluated first; then a flood
    // fill grows from every cell that produced a block, so overhangs and peaks above the
    // estimate are still generated while most of the open sky is never sampled.
    for (int32_t cellYIndex = 0; cellYIndex < cellYCount; cellYIndex++) {
        const int32_t cellY = cellMinY + cellYIndex * OverworldGeneratorConstants::CELL_HEIGHT;
        for (int32_t cellXIndex = 0; cellXIndex < OverworldGeneratorConstants::CELL_X_COUNT; cellXIndex++) {
            for (int32_t cellZIndex = 0; cellZIndex < OverworldGeneratorConstants::CELL_Z_COUNT; cellZIndex++) {
                const int32_t cellIndex = cellIndexOf(cellXIndex, cellYIndex, cellZIndex);
                const int32_t cellX = cellXIndex * OverworldGeneratorConstants::CELL_XZ_SIZE;
                const int32_t cellZ = cellZIndex * OverworldGeneratorConstants::CELL_XZ_SIZE;

                if (!shouldGenerateMandatoryCell(mandatoryTopY, cellX, cellY, cellZ)
                    && !isCornerFloodSeedCell(cellXIndex, cellYIndex, cellY, cellZIndex))
                    continue;

                queued[(size_t) cellIndex] = true;
                solidMandatoryCells[(size_t) cellIndex] = generateCell(cellX, cellY, cellZ);
            }
        }
    }

    for (size_t cellIndex = 0; cellIndex < solidMandatoryCells.size(); cellIndex++) {
        if (!solidMandatoryCells[cellIndex])
            continue;

        const int32_t index = (int32_t) cellIndex;
        const int32_t cellXIndex = index % OverworldGeneratorConstants::CELL_X_COUNT;
        const int32_t cellZIndex = (index / OverworldGeneratorConstants::CELL_X_COUNT)
                                   % OverworldGeneratorConstants::CELL_Z_COUNT;
        const int32_t cellYIndex = index / (OverworldGeneratorConstants::CELL_X_COUNT
                                            * OverworldGeneratorConstants::CELL_Z_COUNT);
        enqueueNeighbors(queue, queued, cellXIndex, cellYIndex, cellZIndex, cellYCount);
    }

    while (!queue.empty()) {
        const int32_t index = queue.front();
        queue.pop_front();

        const int32_t cellXIndex = index % OverworldGeneratorConstants::CELL_X_COUNT;
        const int32_t cellZIndex = (index / OverworldGeneratorConstants::CELL_X_COUNT)
                                   % OverworldGeneratorConstants::CELL_Z_COUNT;
        const int32_t cellYIndex = index / (OverworldGeneratorConstants::CELL_X_COUNT
                                            * OverworldGeneratorConstants::CELL_Z_COUNT);
        const int32_t cellX = cellXIndex * OverworldGeneratorConstants::CELL_XZ_SIZE;
        const int32_t cellZ = cellZIndex * OverworldGeneratorConstants::CELL_XZ_SIZE;
        const int32_t cellY = cellMinY + cellYIndex * OverworldGeneratorConstants::CELL_HEIGHT;

        if (generateCell(cellX, cellY, cellZ))
            enqueueNeighbors(queue, queued, cellXIndex, cellYIndex, cellZIndex, cellYCount);
    }

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

    _generateTerrain(chunk, chunkCache, heightMap);

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++)
            chunk.setHeight(x, z, heightMap[(x << 4) | z]);
    }

    _generateBiomeMap(chunk, chunkCache, heightMap);

    mSurfaceBuilder->apply(chunk);

    mSurfaceOverwriteBuilder->apply(chunk, mSeed);

    chunk.clearDirty();
}
