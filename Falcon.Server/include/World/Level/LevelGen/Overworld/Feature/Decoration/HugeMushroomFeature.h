#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class HugeMushroomFeature : public ObjectGeneratorFeature {
public:
    const char *name() const override;

    ObjectGeneratorPtr getGenerator(IRandom &random) override;

    bool canSpawnHere(int32_t biomeId) const override;

    int32_t getMin() const override;

    int32_t getMax() const override;

    static void setGeneratorFactory(ObjectGeneratorPtr (*factory)());
};
