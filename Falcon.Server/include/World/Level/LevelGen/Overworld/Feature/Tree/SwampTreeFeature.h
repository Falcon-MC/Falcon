#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class SwampTreeFeature : public ObjectGeneratorFeature {
public:
    static const char *NAME;

    const char *name() const override;

    ObjectGeneratorPtr getGenerator(IRandom &random) override;

    bool canSpawnHere(int32_t biomeId) const override;

    int32_t getMin() const override;

    int32_t getMax() const override;

protected:
    bool checkBlock(const BlockState &state, int32_t y) const override;
};
