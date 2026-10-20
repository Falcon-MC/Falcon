#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Decoration/WaterVegetationFeature.h"

class CoralFeature : public WaterVegetationFeature {
protected:
    bool canStay(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level) const override;

    void placeBlock(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level, IRandom &random) override;

    virtual bool placeFeature(LevelChunk &chunk, IRandom &random, int32_t x, int32_t y, int32_t z,
                              const BlockState &coralState) = 0;

    bool placeCoralBlock(LevelChunk &chunk, IRandom &random, int32_t x, int32_t y, int32_t z,
                         const BlockState &coralState);

    static bool inBounds(int32_t x, int32_t y, int32_t z);
};
