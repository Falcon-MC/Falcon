#pragma once

#include "World/Level/LevelGen/ChunkGenerator.h"

#include <cstdint>

// the generator a script's custom dimension is created with: air everywhere, one biome
class VoidGenerator : public ChunkGenerator {
public:
    VoidGenerator(DimensionType dimension, int64_t seed);

    DimensionType getDimensionType() const override {
        return mDimension;
    }

    void generate(LevelChunk &chunk) const override;

    void decorate(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) const override;

    int64_t getSeed() const override {
        return mSeed;
    }

    int getSpawnY() const override;

    int32_t getAirHash() const override {
        return mAirHash;
    }

    int32_t pickBiome(int32_t x, int32_t y, int32_t z) const override;

private:
    DimensionType mDimension;
    int64_t mSeed;
    int32_t mAirHash;
};
