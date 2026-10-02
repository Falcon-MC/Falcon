#pragma once

#include "Level/Generator/Overworld/Feature/Ore/GoldOreMesaFeature.h"

class GoldOreLowerFeature : public GoldOreMesaFeature {
public:
    int32_t getClusterCount() const override;

    bool isRare() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    float getSkipAir() const override;

    bool isHostedOre() const override {
        return true;
    }

    const char *name() const override;
};
