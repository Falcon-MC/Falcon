#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class Level;
class LevelChunk;

class WaterVegetationFeature : public CountGenerateFeature {
public:
    void populate(ChunkGenerateContext &context, IRandom &random) override;

protected:
    virtual bool canStay(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level) const = 0;

    virtual void placeBlock(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level, IRandom &random) = 0;
};
