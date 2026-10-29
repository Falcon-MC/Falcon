#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class LevelChunk;

class SeaPickleFeature : public CountGenerateFeature {
public:
    const char *name() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;

    void populate(ChunkGenerateContext &context, IRandom &random) override;

private:
    static int32_t _findOceanFloorWaterY(LevelChunk &chunk, int32_t x, int32_t z);
};
