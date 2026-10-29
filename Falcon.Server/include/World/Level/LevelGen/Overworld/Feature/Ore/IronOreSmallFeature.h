#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Ore/IronOreMiddleFeature.h"

class IronOreSmallFeature : public IronOreMiddleFeature {
public:
    int32_t getClusterSize() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    ConcentrationType getConcentration() const override;

    const char *name() const override;
};
