#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class MegaTaigaTreeFeature : public GriddedFeature {
public:
    static const char *NAME;

    static const char *ALIAS;

    const char *name() const override;

    ObjectGeneratorPtr getGenerator(IRandom &random) override;

    bool canSpawnHere(int32_t biomeId) const override;

    int32_t getDistanceToNextField() const override;
};
