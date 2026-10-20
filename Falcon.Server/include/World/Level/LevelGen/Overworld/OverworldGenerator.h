#pragma once

#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/Overworld/Biome/OverworldBiomeResult.h"
#include "World/Level/LevelGen/Overworld/OverworldGeneratorConstants.h"
#include "World/Level/LevelGen/Overworld/OverworldNoiseHolder.h"

#include <cstdint>
#include <memory>
#include <string>

class LevelChunk;
class OverworldSurfaceBuilder;
class SurfaceOverwriteBuilder;
class VanillaOverworldBiomeSource;

class OverworldGenerator : public ChunkGenerator {
public:
    explicit OverworldGenerator(int64_t seed);

    ~OverworldGenerator() override;

    OverworldGenerator(const OverworldGenerator &) = delete;

    OverworldGenerator &operator=(const OverworldGenerator &) = delete;

    DimensionType getDimensionType() const override { return DimensionType::Overworld; }

    void generate(LevelChunk &chunk) const override;

    void decorate(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) const override;

    int64_t getSeed() const override { return mSeed; }

    int getSpawnY() const override { return OverworldGeneratorConstants::SEA_LEVEL + 1; }

    int32_t getAirHash() const override { return mAirHash; }

    int32_t pickBiome(int32_t x, int32_t y, int32_t z) const override;

    OverworldBiomeResult pickBiomeResult(int32_t x, int32_t y, int32_t z) const;

    static int64_t parseSeed(const std::string &value);

    static int64_t chunkHash(int32_t chunkX, int32_t chunkZ);

private:
    void _generateTerrain(LevelChunk &chunk, DensityChunkCache &chunkCache, int32_t *heightMap) const;

    void _generateBiomeMap(LevelChunk &chunk, DensityChunkCache &chunkCache, const int32_t *heightMap) const;

    int64_t mSeed;
    int32_t mAirHash;
    std::unique_ptr<OverworldNoiseHolder> mNoiseHolder;
    std::unique_ptr<VanillaOverworldBiomeSource> mBiomeSource;
    std::unique_ptr<OverworldSurfaceBuilder> mSurfaceBuilder;
    std::unique_ptr<SurfaceOverwriteBuilder> mSurfaceOverwriteBuilder;
};
