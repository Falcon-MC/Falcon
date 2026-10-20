#pragma once

#include "World/Level/LevelGen/ChunkGenerator.h"
#include "World/Level/LevelGen/End/Noise/TheEndSimplexNoise.h"
#include "World/Level/LevelGen/End/TheEndGeneratorConstants.h"
#include "World/Level/LevelGen/End/TheEndNoiseHolder.h"

#include <cstdint>
#include <memory>

class LevelChunk;
class TheEndBiomeSource;

class TheEndGenerator : public ChunkGenerator {
public:
    explicit TheEndGenerator(int64_t seed);

    ~TheEndGenerator() override;

    TheEndGenerator(const TheEndGenerator &) = delete;

    TheEndGenerator &operator=(const TheEndGenerator &) = delete;

    DimensionType getDimensionType() const override { return DimensionType::TheEnd; }

    void generate(LevelChunk &chunk) const override;

    void decorate(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) const override;

    int64_t getSeed() const override { return mSeed; }

    int getSpawnY() const override { return TheEndGeneratorConstants::SPAWN_Y; }

    int32_t getAirHash() const override { return mAirHash; }

    int32_t pickBiome(int32_t x, int32_t y, int32_t z) const override;

    static float getIslandHeight(int32_t chunkX, int32_t chunkZ, int32_t x, int32_t z,
                                 const TheEndSimplexNoise &islandNoise);

    static int64_t chunkHash(int32_t chunkX, int32_t chunkZ);

private:
    void _generateTerrain(LevelChunk &chunk, int32_t *heightMap) const;

    int64_t mSeed;
    int32_t mAirHash;
    std::unique_ptr<TheEndNoiseHolder> mNoiseHolder;
    std::unique_ptr<TheEndBiomeSource> mBiomeSource;
};
