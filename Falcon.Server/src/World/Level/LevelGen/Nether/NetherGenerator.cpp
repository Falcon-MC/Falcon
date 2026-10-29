#include "World/Level/LevelGen/Nether/NetherGenerator.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Nether/Biome/VanillaNetherBiomeSource.h"
#include "World/Level/LevelGen/Nether/Feature/NetherFeatureBuilder.h"
#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"
#include "World/Level/LevelGen/Random/SimpleRandom.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "Protocol/BlockStateHasher.h"

#include <algorithm>

namespace {
    const int32_t BIOME_PICK_Y = 64;

    const BlockState &bedrockState() {
        static const BlockState state = VanillaBlocks::BEDROCK().toBlockState();
        return state;
    }

    const BlockState &netherrackState() {
        static const BlockState state = VanillaBlocks::NETHERRACK().toBlockState();
        return state;
    }

    const BlockState &basaltState() {
        static const BlockState state = VanillaBlocks::BASALT().toBlockState();
        return state;
    }

    const BlockState &blackstoneState() {
        static const BlockState state = VanillaBlocks::BLACKSTONE().toBlockState();
        return state;
    }

    const BlockState &gravelState() {
        static const BlockState state = VanillaBlocks::GRAVEL().toBlockState();
        return state;
    }

    const BlockState &soulSandState() {
        static const BlockState state = VanillaBlocks::SOUL_SAND().toBlockState();
        return state;
    }

    const BlockState &soulSoilState() {
        static const BlockState state = VanillaBlocks::SOUL_SOIL().toBlockState();
        return state;
    }

    const BlockState &warpedWartState() {
        static const BlockState state = VanillaBlocks::WARPED_WART_BLOCK().toBlockState();
        return state;
    }

    const BlockState &warpedNyliumState() {
        static const BlockState state = VanillaBlocks::WARPED_NYLIUM().toBlockState();
        return state;
    }

    const BlockState &netherWartBlockState() {
        static const BlockState state = VanillaBlocks::NETHER_WART_BLOCK().toBlockState();
        return state;
    }

    const BlockState &crimsonNyliumState() {
        static const BlockState state = VanillaBlocks::CRIMSON_NYLIUM().toBlockState();
        return state;
    }

    const BlockState &lavaState() {
        static const BlockState state = VanillaBlocks::LAVA().toBlockState();
        return state;
    }

    bool isAirState(const BlockState &state) {
        return state.mName == "minecraft:air";
    }

    bool isNetherrack(const BlockState &state) {
        return state.mName == "minecraft:netherrack";
    }

    int32_t floorDiv(int32_t value, int32_t divisor) {
        int32_t quotient = value / divisor;
        if ((value % divisor != 0) && ((value < 0) != (divisor < 0)))
            quotient--;

        return quotient;
    }

    int32_t clampInt(int32_t value, int32_t min, int32_t max) {
        if (value < min)
            return min;

        if (value > max)
            return max;

        return value;
    }

    bool isTop(const LevelChunk &chunk, int32_t x, int32_t y, int32_t z) {
        for (int32_t i = 0; i < 5; i++) {
            const int32_t sampleY = y + i;
            if (clampInt(sampleY, 1, 127) != sampleY)
                continue;

            if (isAirState(chunk.getBlock(x, sampleY, z)))
                return true;
        }

        return false;
    }

    bool isCeil(const LevelChunk &chunk, int32_t x, int32_t y, int32_t z) {
        for (int32_t i = 0; i < 5; i++) {
            const int32_t sampleY = y - i;
            if (clampInt(sampleY, 1, 127) != sampleY)
                continue;

            if (isAirState(chunk.getBlock(x, sampleY, z)))
                return true;
        }

        return false;
    }
}

NetherGenerator::NetherGenerator(int64_t seed)
        : mSeed(seed), mAirHash(BlockStateHasher::hash("minecraft:air")) {
    mNoiseHolder = std::make_unique<NetherNoiseHolder>(seed);
    mBiomeSource = std::make_unique<VanillaNetherBiomeSource>(mNoiseHolder->getRandomSource());

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

NetherBiomeResult NetherGenerator::pickBiomeResult(int32_t x, int32_t y, int32_t z) const {
    return mBiomeSource->pick(x, y, z);
}

void NetherGenerator::decorate(Level &level, LevelChunk &chunk,
                               std::vector<GeneratedBlockChange> *overflow) const {
    NetherFeatureBuilder::apply(level, chunk, overflow);
}

void NetherGenerator::_generateBiomeMap(LevelChunk &chunk) const {
    const int32_t chunkBaseX = chunk.getX() << 4;
    const int32_t chunkBaseZ = chunk.getZ() << 4;

    int32_t biomes[256];
    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++)
            biomes[(x << 4) | z] = mBiomeSource->pickBiomeId(chunkBaseX + x, BIOME_PICK_Y, chunkBaseZ + z);
    }

    for (int32_t y = NetherGeneratorConstants::MAX_Y; y >= NetherGeneratorConstants::MIN_Y; y--) {
        for (int32_t x = 0; x < 16; x++) {
            for (int32_t z = 0; z < 16; z++)
                chunk.setBiomeAt(x, y, z, (uint32_t) biomes[(x << 4) | z]);
        }
    }
}

void NetherGenerator::_generateTerrain(LevelChunk &chunk, DensityChunkCache &chunkCache) const {
    const NetherTerrainNoises &terrain = mNoiseHolder->getTerrainNoises();
    const DensityFunctionPtr &densityFunction = terrain.getDensityFunction();

    const int32_t chunkBaseX = chunk.getX() << 4;
    const int32_t chunkBaseZ = chunk.getZ() << 4;
    const int32_t minDensityY = NetherGeneratorConstants::MIN_DENSITY_Y;
    const int32_t maxDensityY = NetherGeneratorConstants::MAX_DENSITY_Y;
    const int32_t cellMinY = floorDiv(minDensityY, NetherGeneratorConstants::CELL_HEIGHT)
                             * NetherGeneratorConstants::CELL_HEIGHT;
    const int32_t cellMaxY = floorDiv(maxDensityY, NetherGeneratorConstants::CELL_HEIGHT)
                             * NetherGeneratorConstants::CELL_HEIGHT;

    CellFunctionContext functionContext(&chunkCache);

    for (int32_t cellX = 0; cellX < 16; cellX += NetherGeneratorConstants::CELL_XZ_SIZE) {
        for (int32_t cellZ = 0; cellZ < 16; cellZ += NetherGeneratorConstants::CELL_XZ_SIZE) {
            for (int32_t cellY = cellMaxY; cellY >= cellMinY; cellY -= NetherGeneratorConstants::CELL_HEIGHT) {
                for (int32_t localX = 0; localX < NetherGeneratorConstants::CELL_XZ_SIZE; localX++) {
                    const int32_t x = cellX + localX;
                    const int32_t worldX = chunkBaseX + x;

                    for (int32_t localZ = 0; localZ < NetherGeneratorConstants::CELL_XZ_SIZE; localZ++) {
                        const int32_t z = cellZ + localZ;
                        const int32_t worldZ = chunkBaseZ + z;

                        for (int32_t localY = NetherGeneratorConstants::CELL_HEIGHT - 1; localY >= 0; localY--) {
                            const int32_t y = cellY + localY;
                            if (y < minDensityY || y > maxDensityY)
                                continue;

                            if (densityFunction->compute(functionContext.set(worldX, y, worldZ)) > 0.0)
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

void NetherGenerator::_applySurface(LevelChunk &chunk) const {
    const NetherTerrainNoises &terrain = mNoiseHolder->getTerrainNoises();
    const NormalNoisePtr &netherStateNoise = terrain.getNetherStateNoise();
    const NormalNoisePtr &patchNoise = terrain.getPatchNoise();
    const NormalNoisePtr &netherwartNoise = terrain.getNetherwartNoise();
    const NormalNoisePtr &soulsandNoise = terrain.getSoulsandNoise();

    const int32_t chunkBaseX = chunk.getX() << 4;
    const int32_t chunkBaseZ = chunk.getZ() << 4;

    SimpleRandom random(mSeed ^ chunkHash(chunk.getX(), chunk.getZ()));

    for (int32_t x = 0; x < 16; x++) {
        for (int32_t z = 0; z < 16; z++) {
            const int32_t worldX = chunkBaseX + x;
            const int32_t worldZ = chunkBaseZ + z;
            const int32_t biomeId = (int32_t) chunk.getBiomeAt(x, 0, z);

            chunk.setBlock(x, 0, z, bedrockState());
            chunk.setBlock(x, 126, z, netherrackState());
            chunk.setBlock(x, 127, z, bedrockState());

            for (int32_t y = 1; y < 127; y++) {
                if (biomeId == NetherBiomeIds::BASALT_DELTAS) {
                    if (!isNetherrack(chunk.getBlock(x, y, z)))
                        continue;

                    if (isCeil(chunk, x, y, z)) {
                        chunk.setBlock(x, y, z, basaltState());
                        continue;
                    }

                    if (isAirState(chunk.getBlock(x, y + 1, z))
                        && (netherStateNoise->getValue(worldX, y, worldZ) >= 0.0f
                            || (y <= 35 && y >= 30 && patchNoise->getValue(worldX, y, worldZ) >= -0.012f))) {
                        chunk.setBlock(x, y, z, gravelState());
                        continue;
                    }

                    if (isTop(chunk, x, y, z) || isCeil(chunk, x, y, z))
                        chunk.setBlock(x, y, z, blackstoneState());

                    continue;
                }

                if (biomeId == NetherBiomeIds::SOULSAND_VALLEY) {
                    if (!isNetherrack(chunk.getBlock(x, y, z)))
                        continue;

                    if (isCeil(chunk, x, y, z)) {
                        chunk.setBlock(x, y, z, netherStateNoise->getValue(worldX, y, worldZ) >= 0.0f
                                                ? soulSandState() : soulSoilState());
                        continue;
                    }

                    if (isTop(chunk, x, y, z)) {
                        if (netherStateNoise->getValue(worldX, y, worldZ) >= 0.0f
                            || (y <= 35 && y >= 30 && patchNoise->getValue(worldX, y, worldZ) >= -0.012f))
                            chunk.setBlock(x, y, z, soulSandState());
                        else
                            chunk.setBlock(x, y, z, soulSoilState());
                    }

                    continue;
                }

                if (biomeId == NetherBiomeIds::WARPED_FOREST) {
                    if (isNetherrack(chunk.getBlock(x, y, z))
                        && isAirState(chunk.getBlock(x, y + 1, z))
                        && y > 31
                        && netherStateNoise->getValue(worldX, y, worldZ) <= 0.0f) {
                        chunk.setBlock(x, y, z, netherwartNoise->getValue(worldX, y, worldZ) >= 1.17f
                                                ? warpedWartState() : warpedNyliumState());
                    }

                    continue;
                }

                if (biomeId == NetherBiomeIds::CRIMSON_FOREST) {
                    if (isNetherrack(chunk.getBlock(x, y, z))
                        && isAirState(chunk.getBlock(x, y + 1, z))
                        && y > 31
                        && netherStateNoise->getValue(worldX, y, worldZ) <= 0.54f) {
                        chunk.setBlock(x, y, z, netherwartNoise->getValue(worldX, y, worldZ) >= 1.17f
                                                ? netherWartBlockState() : crimsonNyliumState());
                    }

                    continue;
                }

                if (biomeId == NetherBiomeIds::HELL) {
                    if (!isNetherrack(chunk.getBlock(x, y, z)) || !isTop(chunk, x, y, z))
                        continue;

                    if (y > 31 && y < 35 && soulsandNoise->getValue(worldX, y, worldZ) >= -0.012f) {
                        chunk.setBlock(x, y, z, gravelState());
                        continue;
                    }

                    if (y <= 35 && y >= 30 && soulsandNoise->getValue(worldX, y, worldZ) >= -0.012f)
                        chunk.setBlock(x, y, z, soulSandState());
                }
            }

            for (int32_t i = 0; i < random.nextBoundedInt(6); i++) {
                if (isNetherrack(chunk.getBlock(x, 126 - i, z))
                    && !isAirState(chunk.getBlock(x, 125 - i, z)))
                    chunk.setBlock(x, 126 - i, z, bedrockState());
            }
        }
    }
}

void NetherGenerator::generate(LevelChunk &chunk) const {
    DensityChunkCache chunkCache;
    chunkCache.clear();

    _generateBiomeMap(chunk);

    _generateTerrain(chunk, chunkCache);

    _applySurface(chunk);

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
