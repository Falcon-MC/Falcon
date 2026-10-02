#pragma once

#include "Level/Generator/Overworld/Feature/Ore/IronOreMiddleFeature.h"

class IronOreSmallFeature : public IronOreMiddleFeature {
public:
    int32_t getClusterSize() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    ConcentrationType getConcentration() const override;

    bool isHostedOre() const override {
        return true;
    }

    const char *name() const override;
};
