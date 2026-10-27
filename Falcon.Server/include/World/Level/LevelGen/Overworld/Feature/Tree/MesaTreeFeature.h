#pragma once

#include "World/Level/LevelGen/Feature/FeatureBases.h"

class MesaTreeFeature : public LegacyTreeGeneratorFeature {
public:
    static const char *NAME;

    const char *name() const override;

    TreeGeneratorPtr getGenerator(IRandom &random) override;

    bool canSpawnHere(int32_t biomeId) const override;
};
