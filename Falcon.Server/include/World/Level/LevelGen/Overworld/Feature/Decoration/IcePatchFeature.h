#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class IcePatchFeature : public CountGenerateFeature {
public:
    const char *name() const override;

    int32_t getBase() const override;

    int32_t getRandom() const override;

    void populate(ChunkGenerateContext &context, IRandom &random) override;
};
