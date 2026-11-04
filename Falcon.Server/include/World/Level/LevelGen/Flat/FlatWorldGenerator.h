#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/ChunkGenerator.h"

#include <cstdint>
#include <string>
#include <vector>

/**
 * Overworld generator for worlds whose level.dat sets Generator to flat. Every chunk is the same column
 * built from the FlatWorldLayers JSON, stacked from the bottom of the dimension, with one biome and no
 * features or structures.
 */
class FlatWorldGenerator : public ChunkGenerator {
public:
    FlatWorldGenerator(int64_t seed, const std::string &layers);

    DimensionType getDimensionType() const override { return DimensionType::Overworld; }

    void generate(LevelChunk &chunk) const override;

    void decorate(Level &level, LevelChunk &chunk, std::vector<GeneratedBlockChange> *overflow) const override;

    int64_t getSeed() const override { return mSeed; }

    int getSpawnY() const override { return mSpawnY; }

    int32_t getAirHash() const override { return mAirHash; }

    int32_t pickBiome(int32_t x, int32_t y, int32_t z) const override;

private:
    struct Layer {
        BlockState mBlock;
        int32_t mCount;
    };

    bool _parse(const std::string &layers);

    void _generateEmpty(LevelChunk &chunk) const;

    int64_t mSeed;
    int32_t mAirHash;
    int32_t mBiomeId;
    int32_t mSpawnY;
    std::vector<Layer> mLayers;
    std::vector<BlockState> mColumn;
};
