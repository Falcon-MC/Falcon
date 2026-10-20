#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Ore/CoalOreUpperFeature.h"

class CoalOreLowerFeature : public CoalOreUpperFeature {
public:
    int32_t getClusterCount() const override;

    int32_t getMinHeight() const override;

    int32_t getMaxHeight() const override;

    ConcentrationType getConcentration() const override;

    float getSkipAir() const override;

    const char *name() const override;
};
