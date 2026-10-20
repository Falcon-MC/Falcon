#pragma once

#include "World/Level/LevelGen/Nether/Feature/Ore/AncientDebrisSmallFeature.h"

class AncientDebrisLargeFeature : public AncientDebrisSmallFeature {
public:
    int32_t getClusterCount() const override;

    int32_t getClusterSize() const override;

    int32_t getMaxHeight() const override;

    const char *name() const override;
};
