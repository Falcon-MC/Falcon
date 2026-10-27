#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Decoration/WaterVegetationFeature.h"

class SeagrassFeature : public WaterVegetationFeature {
public:
    const char *name() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;

    virtual float getTallSeagrassProbability() const;

protected:
    bool canStay(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level) const override;

    bool canStay(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, bool tallSeagrass) const;

    void placeBlock(int32_t x, int32_t y, int32_t z, LevelChunk &chunk, Level &level, IRandom &random) override;
};
