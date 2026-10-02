#pragma once

#include "Level/Generator/Overworld/Feature/Ore/CoalOreUpperFeature.h"

class CoalOreLowerFeature : public CoalOreUpperFeature {
public:
    int32_t getClusterCount() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    ConcentrationType getConcentration() const override;

    float getSkipAir() const override;

    bool isHostedOre() const override {
        return true;
    }

    const char *name() const override;
};
