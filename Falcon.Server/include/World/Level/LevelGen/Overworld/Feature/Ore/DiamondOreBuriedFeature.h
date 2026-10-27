#pragma once

#include "World/Level/LevelGen/Overworld/Feature/Ore/DiamondOreFeature.h"

class DiamondOreBuriedFeature : public DiamondOreFeature {
public:
    int32_t getClusterCount() const override;

    int32_t getClusterSize() const override;

    float getSkipAir() const override;

    const char *name() const override;
};
