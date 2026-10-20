#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class Level;
class LevelChunk;

class DiscFeature : public CountGenerateFeature {
public:
    virtual BlockState getSourceBlock() const = 0;

    virtual int32_t getMinRadius() const = 0;

    virtual int32_t getMaxRadius() const = 0;

    virtual double getProbability() const;

    int32_t getBase() const override;

    int32_t getRandom() const override;

    void populate(ChunkGenerateContext &context, IRandom &random) override;

    virtual bool isSupportValid(const BlockState &support, Level &level, int32_t x, int32_t y, int32_t z) const;

    virtual int32_t getY(LevelChunk &chunk, Level &level, int32_t x, int32_t z) const;
};
