#pragma once

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Feature/FeatureBases.h"

#include <vector>

class DiscGenerateFeature : public CountGenerateFeature {
public:
    virtual const BlockState &getSourceBlock() const = 0;

    virtual int32_t getMinRadius() const = 0;

    virtual int32_t getMaxRadius() const = 0;

    virtual int32_t getRadiusY() const = 0;

    virtual double getProbability() const;

    virtual const std::vector<const BlockState *> &getReplacementBlocks() const;

    int32_t getRandom() const override;

    void populate(ChunkGenerateContext &context, IRandom &random) override;

protected:
    static const BlockState &stillWaterState();

    static const BlockState &stoneState();

    static const BlockState &dirtState();

    static const BlockState &grassState();
};
