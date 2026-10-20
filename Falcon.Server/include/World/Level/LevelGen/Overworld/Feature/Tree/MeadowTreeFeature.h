#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Tree/PlainsTreeFeature.h"

class MeadowTreeFeature : public PlainsTreeFeature {
public:
    static const char *NAME;

    const char *name() const override;

    float getBeeNestChance() const override;

    bool canSpawnHere(int32_t biomeId) const override;
};
