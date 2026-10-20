#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Tree/ForestTreeFeature.h"

class FlowerForestTreeFeature : public ForestTreeFeature {
public:
    static const char *NAME;

    const char *name() const override;

    TreeGeneratorPtr getGenerator(IRandom &random) override;

    int32_t getMin() const override;

    int32_t getMax() const override;

    float getBeeNestChance() const override;

    bool canSpawnHere(int32_t biomeId) const override;
};
