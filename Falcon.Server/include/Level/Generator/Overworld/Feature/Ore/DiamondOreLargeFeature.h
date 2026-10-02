#pragma once

#include "Level/Generator/Overworld/Feature/Ore/DiamondOreFeature.h"

class DiamondOreLargeFeature : public DiamondOreFeature {
public:
    int32_t getClusterCount() const override;

    int32_t getClusterSize() const override;

    float getSkipAir() const override;

    bool isRare() const override;

    bool isHostedOre() const override {
        return true;
    }

    const char *name() const override;
};
