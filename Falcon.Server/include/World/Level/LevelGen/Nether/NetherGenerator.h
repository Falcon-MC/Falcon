#pragma once

#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Density/DensityFunction.h"
#include "World/Level/LevelGen/Nether/Biome/NetherBiomeResult.h"
#include "World/Level/LevelGen/Nether/NetherGeneratorConstants.h"
#include "World/Level/LevelGen/Nether/NetherNoiseHolder.h"

#include <cstdint>
#include <memory>

class LevelChunk;
class VanillaNetherBiomeSource;

class NetherGenerator : public ChunkGenerator {
public:
    explicit NetherGenerator(int64_t seed);

    ~NetherGenerator() override;

    NetherGenerator(const NetherGenerator &) = delete;

    NetherGenerator &operator=(const NetherGenerator &) = delete;

    DimensionType getDimensionType() const override { return DimensionType::Nether; }

    void generate(LevelChunk &chunk) const override;

    void decorate(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) const override;

    int64_t getSeed() const override { return mSeed; }

    int getSpawnY() const override { return NetherGeneratorConstants::LAVA_LEVEL + 1; }

    int32_t getAirHash() const override { return mAirHash; }

    int32_t pickBiome(int32_t x, int32_t y, int32_t z) const override;

    NetherBiomeResult pickBiomeResult(int32_t x, int32_t y, int32_t z) const;

    static int64_t chunkHash(int32_t chunkX, int32_t chunkZ);

private:
    void _generateBiomeMap(LevelChunk &chunk) const;

    void _generateTerrain(LevelChunk &chunk, DensityChunkCache &chunkCache) const;

    void _applySurface(LevelChunk &chunk) const;

    int64_t mSeed;
    int32_t mAirHash;
    std::unique_ptr<NetherNoiseHolder> mNoiseHolder;
    std::unique_ptr<VanillaNetherBiomeSource> mBiomeSource;
};
