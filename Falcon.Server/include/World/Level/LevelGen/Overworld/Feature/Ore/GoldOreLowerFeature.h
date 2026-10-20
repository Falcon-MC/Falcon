#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Ore/GoldOreMesaFeature.h"

class GoldOreLowerFeature : public GoldOreMesaFeature {
public:
    int32_t getClusterCount() const override;

    bool isRare() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    float getSkipAir() const override;

    const char *name() const override;
};
